#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <time.h>
#include <stdarg.h>

static int wallpaper_pause_state = -1;
static long long last_wallpaper_command_ms = 0;

long long monotonic_ms() {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        return 0;
    }
    return ((long long)ts.tv_sec * 1000LL) + (ts.tv_nsec / 1000000LL);
}

void get_ipc_socket_path(char *dest, size_t size) {
    const char *runtime_dir = getenv("XDG_RUNTIME_DIR");
    if (runtime_dir && runtime_dir[0] != '\0') {
        snprintf(dest, size, "%s/mpvpaper-ipc", runtime_dir);
    } else {
        snprintf(dest, size, "/tmp/mpvpaper-ipc");
    }
}

void log_message(const char *fmt, ...) {
    char state_dir[512];
    const char *state_home = getenv("XDG_STATE_HOME");
    if (state_home && state_home[0] != '\0') {
        snprintf(state_dir, sizeof(state_dir), "%s/anto426", state_home);
    } else {
        const char *home = getenv("HOME");
        snprintf(state_dir, sizeof(state_dir), "%s/.local/state/anto426", home ? home : "/root");
    }
    
    char mkdir_cmd[2048];
    snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p \"%s\"", state_dir);
    system(mkdir_cmd);
    
    char log_file[2048];
    snprintf(log_file, sizeof(log_file), "%s/wallpaper_daemon.log", state_dir);
    
    FILE *f = fopen(log_file, "a");
    if (f) {
        time_t t = time(NULL);
        struct tm *tm_info = localtime(&t);
        char time_str[64];
        strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);
        
        fprintf(f, "[%s] ", time_str);
        va_list args;
        va_start(args, fmt);
        vfprintf(f, fmt, args);
        va_end(args);
        fprintf(f, "\n");
        fclose(f);
    }
}

int is_process_running(const char *proc_name) {
    DIR *dir = opendir("/proc");
    if (!dir) return 0;
    struct dirent *entry;
    int found = 0;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_DIR) {
            char *endptr;
            long pid = strtol(entry->d_name, &endptr, 10);
            if (*endptr == '\0') {
                char comm_path[256];
                snprintf(comm_path, sizeof(comm_path), "/proc/%ld/comm", pid);
                FILE *f = fopen(comm_path, "r");
                if (f) {
                    char comm[256];
                    if (fgets(comm, sizeof(comm), f)) {
                        size_t len = strlen(comm);
                        while (len > 0 && (comm[len-1] == '\n' || comm[len-1] == '\r')) {
                            comm[len-1] = '\0';
                            len--;
                        }
                        if (strcmp(comm, proc_name) == 0) {
                            found = 1;
                            fclose(f);
                            break;
                        }
                    }
                    fclose(f);
                }
            }
        }
    }
    closedir(dir);
    return found;
}

void send_mpvpaper_command(const char *cmd_json) {
    char ipc_socket[1024];
    get_ipc_socket_path(ipc_socket, sizeof(ipc_socket));

    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return;

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    
    // Safely copy socket path with size assertion to prevent warnings
    if (strlen(ipc_socket) < sizeof(addr.sun_path)) {
        strcpy(addr.sun_path, ipc_socket);
    } else {
        memcpy(addr.sun_path, ipc_socket, sizeof(addr.sun_path) - 1);
        addr.sun_path[sizeof(addr.sun_path) - 1] = '\0';
    }

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) == 0) {
        write(fd, cmd_json, strlen(cmd_json));
    }
    close(fd);
}

void set_wallpaper_paused(int paused) {
    long long now;
    long long elapsed;

    if (wallpaper_pause_state == paused) {
        return;
    }

    now = monotonic_ms();
    if (last_wallpaper_command_ms > 0 && now > 0) {
        elapsed = now - last_wallpaper_command_ms;
        if (elapsed >= 0 && elapsed < 300) {
            usleep((useconds_t)((300 - elapsed) * 1000));
            now = monotonic_ms();
        }
    }

    if (paused) {
        send_mpvpaper_command("{\"command\": [\"set_property\", \"pause\", true]}\n");
    } else {
        send_mpvpaper_command("{\"command\": [\"set_property\", \"pause\", false]}\n");
    }
    wallpaper_pause_state = paused;
    last_wallpaper_command_ms = now;
}

void pause_wallpaper() {
    set_wallpaper_paused(1);
}

void resume_wallpaper() {
    set_wallpaper_paused(0);
}

int wallpaper_auto_pause_enabled() {
    const char *value = getenv("ANTO426_WALLPAPER_AUTO_PAUSE");
    if (!value || value[0] == '\0') {
        return 0;
    }
    return strcmp(value, "0") != 0 &&
           strcmp(value, "false") != 0 &&
           strcmp(value, "FALSE") != 0 &&
           strcmp(value, "no") != 0 &&
           strcmp(value, "NO") != 0 &&
           strcmp(value, "off") != 0 &&
           strcmp(value, "OFF") != 0;
}

void update_state() {
    if (!is_process_running("mpvpaper")) {
        return;
    }
    if (!wallpaper_auto_pause_enabled()) {
        resume_wallpaper();
        return;
    }

    if (is_process_running("hyprlock")) {
        pause_wallpaper();
        return;
    }

    FILE *p_ws = popen("hyprctl activeworkspace -j 2>/dev/null | jq -r '[.hasfullscreen, .name] | @tsv'", "r");
    if (!p_ws) return;

    char has_fs_str[64] = "false";
    char ws_name[256] = "";
    if (fscanf(p_ws, "%63s %255s", has_fs_str, ws_name) < 1) {
        // Ignore failure
    }
    pclose(p_ws);

    if (strcmp(has_fs_str, "true") == 0) {
        pause_wallpaper();
        return;
    }

    if (ws_name[0] != '\0') {
        char jq_cmd[1024];
        snprintf(jq_cmd, sizeof(jq_cmd), 
                 "hyprctl clients -j 2>/dev/null | jq -r --arg ws \"%s\" "
                 "'map(select(.workspace.name == $ws and .floating == false)) | length'", 
                 ws_name);
        FILE *p_count = popen(jq_cmd, "r");
        if (p_count) {
            int tiled_count = 0;
            if (fscanf(p_count, "%d", &tiled_count) == 1) {
                if (tiled_count > 0) {
                    pause_wallpaper();
                    pclose(p_count);
                    return;
                }
            }
            pclose(p_count);
        }
    }

    resume_wallpaper();
}

void run_event_loop(const char *socket_path) {
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        log_message("Failed to create socket, falling back to polling");
        while (1) {
            update_state();
            sleep(2);
        }
        return;
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        close(fd);
        log_message("Failed to connect to Hyprland socket %s, falling back to polling", socket_path);
        while (1) {
            update_state();
            sleep(2);
        }
        return;
    }

    log_message("Connected to Hyprland event socket: %s", socket_path);
    update_state();

    char buffer[4096];
    ssize_t bytes_read;
    char line_buf[4096];
    size_t line_len = 0;

    while ((bytes_read = read(fd, buffer, sizeof(buffer))) > 0) {
        for (ssize_t i = 0; i < bytes_read; i++) {
            char c = buffer[i];
            if (c == '\n' || c == '\r') {
                if (line_len > 0) {
                    line_buf[line_len] = '\0';
                    
                    if (strncmp(line_buf, "fullscreen>>", 12) == 0 ||
                        strncmp(line_buf, "workspace>>", 11) == 0 ||
                        strncmp(line_buf, "openwindow>>", 12) == 0 ||
                        strncmp(line_buf, "closewindow>>", 13) == 0 ||
                        strncmp(line_buf, "activewindow>>", 14) == 0 ||
                        strncmp(line_buf, "changefloatingmode>>", 20) == 0) {
                        
                        update_state();
                    }
                    
                    line_len = 0;
                }
            } else {
                if (line_len < sizeof(line_buf) - 1) {
                    line_buf[line_len++] = c;
                }
            }
        }
    }

    close(fd);
    log_message("Hyprland event socket closed, reconnecting...");
}

int main() {
    if (daemon(1, 1) != 0) {
        // Ignore error
    }

    log_message("Wallpaper daemon started (C version)");

    char hypr_socket[1024] = "";
    const char *hypr_signature = getenv("HYPRLAND_INSTANCE_SIGNATURE");
    if (hypr_signature && hypr_signature[0] != '\0') {
        const char *runtime_dir = getenv("XDG_RUNTIME_DIR");
        char runtime_socket[1024];
        char legacy_socket[1024];

        snprintf(runtime_socket, sizeof(runtime_socket), "%s/hypr/%s/.socket2.sock",
                 runtime_dir && runtime_dir[0] != '\0' ? runtime_dir : "/tmp",
                 hypr_signature);
        snprintf(legacy_socket, sizeof(legacy_socket), "/tmp/hypr/%s/.socket2.sock", hypr_signature);

        if (access(runtime_socket, F_OK) == 0) {
            snprintf(hypr_socket, sizeof(hypr_socket), "%s", runtime_socket);
        } else {
            snprintf(hypr_socket, sizeof(hypr_socket), "%s", legacy_socket);
        }
    }

    while (1) {
        if (!wallpaper_auto_pause_enabled()) {
            update_state();
            sleep(30);
            continue;
        }

        if (hypr_socket[0] != '\0' && access(hypr_socket, F_OK) == 0) {
            run_event_loop(hypr_socket);
        } else {
            update_state();
            sleep(2);
        }
        sleep(1);
    }

    return 0;
}
