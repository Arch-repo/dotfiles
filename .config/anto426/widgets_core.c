#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <signal.h>
#include <ctype.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/wait.h>

#define MAX_LINE 4096
#define MAX_VAL 2048
#define MAX_WIDGETS 128

char CONFIG_FILE[1024];
char LAYOUT_FILE[1024];
char RUNTIME_DIR[1024];

// Function Prototypes
void get_env_path(char *dest, size_t size, const char *rel_path);
void get_runtime_path(char *dest, size_t size, const char *filename);
void init_global_paths(void);
int mkdir_p(const char *path);
void notify(const char *message);
void trim(char *str);
void remove_quotes(char *str);
void decode_env_value(char *str);
void write_env_quoted(FILE *f, const char *key, const char *value, int export_prefix);
void regex_escape(const char *src, char *dest, size_t dest_size);
int read_env_value(const char *filepath, const char *key, char *dest, size_t dest_size);
void save_custom_widget_ids_str(const char *str);
int get_custom_widget_ids_str(char *dest, size_t size);
int token_list_contains(const char *list, const char *token);
void append_unique_token(char *dest, size_t size, const char *token);
void get_enabled_widget_ids_str(char *dest, size_t size);
void get_known_widget_ids_str(char *dest, size_t size);
void get_widget_order_str(char *dest, size_t size);
void make_layout_key(const char *widget_id, const char *field, char *dest, size_t size);
void append_truncated(char *dest, size_t size, const char *src);
int read_layout_value(const char *widget_id, const char *field, char *dest, size_t size);
void remove_widget_layout_vars(const char *widget_id);
int get_custom_widget_meta(const char *widget_id, const char *field, char *dest, size_t size);
const char *get_widget_label(const char *widget_id);
int get_widget_class(const char *widget_id, char *dest, size_t size);
int is_music_active(void);
int get_terminal(char *dest, size_t size);
int get_widget_address(const char *widget_id, char *dest, size_t dest_size);
int is_widget_running(const char *widget_id);
void dedupe_widget_windows(const char *widget_id);
void get_widget_meta_defaults(const char *widget_id, char *w, size_t w_sz, char *h, size_t h_sz, char *dx, size_t dx_sz, char *dy, size_t dy_sz);
int launch_terminal(const char *name, const char *wm_class, const char *title, const char *command);
void write_custom_widget_env(const char *widget_id, const char *name, const char *wm_class, const char *command, const char *mode, const char *w, const char *h, const char *monitor);
int launch_custom_widget(const char *widget_id, int locked);
void stop_widget(const char *widget_id, const char *wm_class);
void stop_custom_widget(const char *widget_id);
void ensure_config(void);
void update_cava_colors(void);
void write_hypr_rules(int locked);
const char *get_clock_command(void);
const char *get_cava_command(void);
const char *get_system_command(void);
void apply_widget_layout_single(const char *widget_id, const char *monitor, int active_ws, int locked, const char *addr_arg);
void apply_widget_layouts(void);
void show_all_widgets_unlocked(void);
int save_widget_layouts(void);
void start_widgets(void);
void stop_widgets(void);
void start_daemon_process(void);
void stop_daemon_process(void);
void run_daemon(void);
int is_any_running(void);
void reap_children(void);

void get_env_path(char *dest, size_t size, const char *rel_path) {
    const char *config_home = getenv("XDG_CONFIG_HOME");
    if (config_home && config_home[0] != '\0') {
        snprintf(dest, size, "%s/%s", config_home, rel_path);
    } else {
        const char *home = getenv("HOME");
        snprintf(dest, size, "%s/.config/%s", home ? home : "/root", rel_path);
    }
}

void get_runtime_path(char *dest, size_t size, const char *filename) {
    const char *runtime_dir = getenv("XDG_RUNTIME_DIR");
    if (runtime_dir && runtime_dir[0] != '\0') {
        snprintf(dest, size, "%s/anto426-widgets/%s", runtime_dir, filename);
    } else {
        snprintf(dest, size, "/tmp/anto426-widgets/%s", filename);
    }
}

void init_global_paths() {
    get_env_path(CONFIG_FILE, sizeof(CONFIG_FILE), "anto426/widgets.env");
    get_env_path(LAYOUT_FILE, sizeof(LAYOUT_FILE), "anto426/widgets_layout.env");
    get_runtime_path(RUNTIME_DIR, sizeof(RUNTIME_DIR), "");
    mkdir_p(RUNTIME_DIR);
}

int mkdir_p(const char *path) {
    char tmp[1024];
    size_t len;

    if (!path || path[0] == '\0') return 0;
    snprintf(tmp, sizeof(tmp), "%s", path);
    len = strlen(tmp);
    while (len > 1 && tmp[len - 1] == '/') {
        tmp[len - 1] = '\0';
        len--;
    }

    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            mkdir(tmp, 0755);
            *p = '/';
        }
    }
    return mkdir(tmp, 0755) == 0 || access(tmp, F_OK) == 0;
}

void notify(const char *message) {
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "notify-send \"Widget\" \"%s\" 2>/dev/null || true", message);
    system(cmd);
}

void trim(char *str) {
    char *start = str;
    char *end;

    while (isspace((unsigned char)*start)) start++;
    if (start != str) {
        memmove(str, start, strlen(start) + 1);
    }
    if (*str == '\0') return;

    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
}

void remove_quotes(char *str) {
    size_t len = strlen(str);
    if (len >= 2) {
        if ((str[0] == '"' && str[len - 1] == '"') || (str[0] == '\'' && str[len - 1] == '\'')) {
            memmove(str, str + 1, len - 2);
            str[len - 2] = '\0';
        }
    }
}

void decode_env_value(char *str) {
    char quote = '\0';
    size_t len;
    size_t r = 0;
    size_t w = 0;

    trim(str);
    len = strlen(str);
    if (len >= 2 && ((str[0] == '"' && str[len - 1] == '"') || (str[0] == '\'' && str[len - 1] == '\''))) {
        quote = str[0];
        memmove(str, str + 1, len - 2);
        str[len - 2] = '\0';
    }

    while (str[r]) {
        if (quote == '"' && str[r] == '\\' && str[r + 1] != '\0') {
            char next = str[r + 1];
            if (next == '"' || next == '\\' || next == '$' || next == '`') {
                str[w++] = next;
                r += 2;
                continue;
            }
        } else if (quote == '\0' && str[r] == '\\' && str[r + 1] != '\0') {
            str[w++] = str[r + 1];
            r += 2;
            continue;
        }
        str[w++] = str[r++];
    }
    str[w] = '\0';
}

void write_env_quoted(FILE *f, const char *key, const char *value, int export_prefix) {
    fprintf(f, "%s%s=\"", export_prefix ? "export " : "", key);
    for (size_t i = 0; value && value[i]; i++) {
        char c = value[i];
        if (c == '\\' || c == '"' || c == '$' || c == '`') {
            fputc('\\', f);
            fputc(c, f);
        } else if (c == '\n') {
            fputs("\\n", f);
        } else {
            fputc(c, f);
        }
    }
    fputs("\"\n", f);
}

void regex_escape(const char *src, char *dest, size_t dest_size) {
    size_t j = 0;

    if (dest_size == 0) return;
    for (size_t i = 0; src && src[i] && j + 1 < dest_size; i++) {
        if (strchr(".[]\\*^$()+?{}|", src[i]) && j + 2 < dest_size) {
            dest[j++] = '\\';
        }
        dest[j++] = src[i];
    }
    dest[j] = '\0';
}

int read_env_value(const char *filepath, const char *key, char *dest, size_t dest_size) {
    dest[0] = '\0';
    FILE *f = fopen(filepath, "r");
    if (!f) return 0;

    char line[MAX_LINE];
    int found = 0;
    while (fgets(line, sizeof(line), f)) {
        char *ptr = line;
        while (isspace((unsigned char)*ptr)) ptr++;
        if (*ptr == '\0' || *ptr == '#') continue;

        if (strncmp(ptr, "export ", 7) == 0) {
            ptr += 7;
        }

        char *eq = strchr(ptr, '=');
        if (eq) {
            *eq = '\0';
            char cur_key[MAX_LINE];
            strncpy(cur_key, ptr, sizeof(cur_key) - 1);
            cur_key[sizeof(cur_key) - 1] = '\0';
            trim(cur_key);

            if (strcmp(cur_key, key) == 0) {
                char cur_val[MAX_VAL];
                snprintf(cur_val, sizeof(cur_val), "%s", eq + 1);
                decode_env_value(cur_val);
                snprintf(dest, dest_size, "%s", cur_val);
                found = 1;
                break;
            }
        }
    }
    fclose(f);
    return found;
}

void save_custom_widget_ids_str(const char *str) {
    char custom_f[512];
    get_env_path(custom_f, sizeof(custom_f), "anto426/widgets_custom.env");
    FILE *f = fopen(custom_f, "w");
    if (f) {
        fprintf(f, "# Custom widgets list. Per-widget config lives in ~/.config/anto426/widgets.d/<id>.env\n");
        write_env_quoted(f, "ANTO426_CUSTOM_WIDGETS", str, 1);
        fclose(f);
    }
}

int get_custom_widget_ids_str(char *dest, size_t size) {
    char custom_f[512];
    get_env_path(custom_f, sizeof(custom_f), "anto426/widgets_custom.env");
    return read_env_value(custom_f, "ANTO426_CUSTOM_WIDGETS", dest, size);
}

int token_list_contains(const char *list, const char *token) {
    size_t token_len;
    const char *p;

    if (!list || !token || token[0] == '\0') return 0;
    token_len = strlen(token);
    p = list;
    while (*p) {
        const char *start;
        size_t len;

        while (*p && isspace((unsigned char)*p)) p++;
        start = p;
        while (*p && !isspace((unsigned char)*p)) p++;
        len = (size_t)(p - start);
        if (len == token_len && strncmp(start, token, len) == 0) {
            return 1;
        }
    }
    return 0;
}

void append_unique_token(char *dest, size_t size, const char *token) {
    size_t len;

    if (!dest || !token || token[0] == '\0' || token_list_contains(dest, token)) return;
    len = strlen(dest);
    if (len > 0 && len + 1 < size) {
        strncat(dest, " ", size - len - 1);
        len++;
    }
    if (len < size) {
        strncat(dest, token, size - len - 1);
    }
}

void get_enabled_widget_ids_str(char *dest, size_t size) {
    char config_f[512];

    dest[0] = '\0';
    get_env_path(config_f, sizeof(config_f), "anto426/widgets.env");
    if (!read_env_value(config_f, "ANTO426_WIDGETS_ENABLED", dest, size)) {
        snprintf(dest, size, "clock cava system");
    }
}

void get_known_widget_ids_str(char *dest, size_t size) {
    char enabled_str[1024] = "";
    char customs_str[2048] = "";
    char copy[4096];
    char *tok;
    char *saveptr = NULL;

    dest[0] = '\0';
    get_enabled_widget_ids_str(enabled_str, sizeof(enabled_str));
    get_custom_widget_ids_str(customs_str, sizeof(customs_str));

    snprintf(copy, sizeof(copy), "%s %s", enabled_str, customs_str);
    tok = strtok_r(copy, " \t\r\n", &saveptr);
    while (tok) {
        append_unique_token(dest, size, tok);
        tok = strtok_r(NULL, " \t\r\n", &saveptr);
    }
}

void get_widget_order_str(char *dest, size_t size) {
    char layout_f[512];
    char saved_order[4096] = "";
    char known[4096] = "";
    char copy[4096];
    char *tok;
    char *saveptr = NULL;

    dest[0] = '\0';
    get_env_path(layout_f, sizeof(layout_f), "anto426/widgets_layout.env");
    read_env_value(layout_f, "ANTO426_WIDGET_ORDER", saved_order, sizeof(saved_order));
    get_known_widget_ids_str(known, sizeof(known));

    snprintf(copy, sizeof(copy), "%s", saved_order);
    tok = strtok_r(copy, " \t\r\n", &saveptr);
    while (tok) {
        if (token_list_contains(known, tok)) {
            append_unique_token(dest, size, tok);
        }
        tok = strtok_r(NULL, " \t\r\n", &saveptr);
    }

    snprintf(copy, sizeof(copy), "%s", known);
    saveptr = NULL;
    tok = strtok_r(copy, " \t\r\n", &saveptr);
    while (tok) {
        append_unique_token(dest, size, tok);
        tok = strtok_r(NULL, " \t\r\n", &saveptr);
    }
}

void append_truncated(char *dest, size_t size, const char *src) {
    size_t len;

    if (size == 0 || !src) return;
    len = strlen(dest);
    if (len >= size - 1) return;
    strncat(dest, src, size - len - 1);
}

void make_layout_key(const char *widget_id, const char *field, char *dest, size_t size) {
    if (size == 0) return;
    dest[0] = '\0';
    append_truncated(dest, size, "LAYOUT_");
    append_truncated(dest, size, widget_id);
    append_truncated(dest, size, "_");
    append_truncated(dest, size, field);
    for (size_t i = 0; dest[i]; i++) {
        dest[i] = (char)toupper((unsigned char)dest[i]);
    }
}

int read_layout_value(const char *widget_id, const char *field, char *dest, size_t size) {
    char layout_f[512];
    char key[128];

    get_env_path(layout_f, sizeof(layout_f), "anto426/widgets_layout.env");
    make_layout_key(widget_id, field, key, sizeof(key));
    return read_env_value(layout_f, key, dest, size);
}

void remove_widget_layout_vars(const char *widget_id) {
    char layout_f[512];
    char tmp_f[1024];
    char prefix[128];
    FILE *fin;
    FILE *fout;

    get_env_path(layout_f, sizeof(layout_f), "anto426/widgets_layout.env");
    snprintf(tmp_f, sizeof(tmp_f), "%s.tmp", layout_f);
    snprintf(prefix, sizeof(prefix), "LAYOUT_%s_", widget_id);
    for (size_t i = 0; prefix[i]; i++) {
        prefix[i] = (char)toupper((unsigned char)prefix[i]);
    }

    fin = fopen(layout_f, "r");
    if (!fin) return;
    fout = fopen(tmp_f, "w");
    if (!fout) {
        fclose(fin);
        return;
    }

    char line[MAX_LINE];
    while (fgets(line, sizeof(line), fin)) {
        char original[MAX_LINE];
        char *ptr;
        char *eq;

        snprintf(original, sizeof(original), "%s", line);
        ptr = line;
        while (isspace((unsigned char)*ptr)) ptr++;
        if (strncmp(ptr, "export ", 7) == 0) ptr += 7;
        eq = strchr(ptr, '=');
        if (!eq) {
            fputs(original, fout);
            continue;
        }

        *eq = '\0';
        trim(ptr);
        for (size_t i = 0; ptr[i]; i++) {
            ptr[i] = (char)toupper((unsigned char)ptr[i]);
        }

        if (strcmp(ptr, "ANTO426_WIDGET_ORDER") == 0) {
            char val[MAX_VAL];
            char new_order[4096] = "";
            char *tok;
            char *saveptr = NULL;

            snprintf(val, sizeof(val), "%s", eq + 1);
            decode_env_value(val);
            tok = strtok_r(val, " \t\r\n", &saveptr);
            while (tok) {
                if (strcmp(tok, widget_id) != 0) {
                    append_unique_token(new_order, sizeof(new_order), tok);
                }
                tok = strtok_r(NULL, " \t\r\n", &saveptr);
            }
            write_env_quoted(fout, "ANTO426_WIDGET_ORDER", new_order, 1);
            continue;
        }

        if (strncmp(ptr, prefix, strlen(prefix)) == 0) {
            continue;
        }
        fputs(original, fout);
    }

    fclose(fin);
    fclose(fout);
    rename(tmp_f, layout_f);
}

int get_custom_widget_meta(const char *widget_id, const char *field, char *dest, size_t size) {
    char env_f[512];
    get_env_path(env_f, sizeof(env_f), "");
    char fpath[1024];
    snprintf(fpath, sizeof(fpath), "%s/anto426/widgets.d/%.200s.env", env_f, widget_id);
    
    char key[64];
    snprintf(key, sizeof(key), "ANTO426_WIDGET_%s", field);
    for (int i=0; key[i]; i++) key[i] = toupper((unsigned char)key[i]);
    
    return read_env_value(fpath, key, dest, size);
}

const char *get_widget_label(const char *widget_id) {
    static char buf[256];
    if (strcmp(widget_id, "clock") == 0) return "Clock Widget";
    if (strcmp(widget_id, "cava") == 0) return "Cava Audio Visualizer";
    if (strcmp(widget_id, "system") == 0) return "System Monitor";
    
    if (get_custom_widget_meta(widget_id, "NAME", buf, sizeof(buf))) {
        return buf;
    }
    
    snprintf(buf, sizeof(buf), "%s", widget_id);
    for (int i=0; buf[i]; i++) {
        if (buf[i] == '_') buf[i] = ' ';
    }
    buf[0] = toupper((unsigned char)buf[0]);
    return buf;
}

int get_widget_class(const char *widget_id, char *dest, size_t size) {
    if (strcmp(widget_id, "clock") == 0) {
        snprintf(dest, size, "anto426.widget.clock");
        return 1;
    } else if (strcmp(widget_id, "cava") == 0) {
        snprintf(dest, size, "anto426.widget.cava");
        return 1;
    } else if (strcmp(widget_id, "system") == 0) {
        snprintf(dest, size, "anto426.widget.system");
        return 1;
    } else {
        if (get_custom_widget_meta(widget_id, "CLASS", dest, size)) {
            return 1;
        }
    }
    snprintf(dest, size, "anto426.widget.cmd.%s", widget_id);
    return 1;
}

int is_music_active() {
    char buf[256];
    FILE *p;
    
    p = popen("playerctl status 2>/dev/null", "r");
    if (p) {
        if (fgets(buf, sizeof(buf), p)) {
            if (strstr(buf, "Playing")) {
                pclose(p);
                return 1;
            }
        }
        pclose(p);
    }
    
    p = popen("pactl list sink-inputs 2>/dev/null", "r");
    if (p) {
        while (fgets(buf, sizeof(buf), p)) {
            if (strstr(buf, "Corked: no")) {
                pclose(p);
                return 1;
            }
        }
        pclose(p);
    }
    
    p = popen("wpctl status 2>/dev/null", "r");
    if (p) {
        while (fgets(buf, sizeof(buf), p)) {
            if (strstr(buf, "[active]")) {
                int exclude = 0;
                char *terms[] = {"cava", "input", "mic", "source"};
                for (int i = 0; i < 4; i++) {
                    if (strcasestr(buf, terms[i])) {
                        exclude = 1;
                        break;
                    }
                }
                if (!exclude) {
                    pclose(p);
                    return 1;
                }
            }
        }
        pclose(p);
    }
    
    return 0;
}

int get_terminal(char *dest, size_t size) {
    const char *terms[] = {"ghostty", "kitty", "foot", "alacritty"};
    for (int i = 0; i < 4; i++) {
        char cmd[128];
        snprintf(cmd, sizeof(cmd), "which %s >/dev/null 2>&1", terms[i]);
        if (system(cmd) == 0) {
            snprintf(dest, size, "%s", terms[i]);
            return 1;
        }
    }
    return 0;
}

int get_widget_address(const char *widget_id, char *dest, size_t dest_size) {
    dest[0] = '\0';

    char pid_dir[512];
    get_runtime_path(pid_dir, sizeof(pid_dir), "");
    char pid_path[1024];
    snprintf(pid_path, sizeof(pid_path), "%s/%s.pid", pid_dir, widget_id);
    FILE *f_pid = fopen(pid_path, "r");
    if (f_pid) {
        int pid = 0;
        if (fscanf(f_pid, "%d", &pid) == 1 && kill(pid, 0) == 0) {
            fclose(f_pid);
            char cmd_pid[1024];
            snprintf(cmd_pid, sizeof(cmd_pid), "hyprctl clients -j | jq -r --argjson pid %d '.[] | select(.pid == $pid) | .address' | head -n1", pid);
            FILE *p_pid = popen(cmd_pid, "r");
            if (p_pid) {
                char buf[64];
                if (fgets(buf, sizeof(buf), p_pid)) {
                    trim(buf);
                    if (buf[0] != '\0') {
                        snprintf(dest, dest_size, "%s", buf);
                        pclose(p_pid);
                        return 1;
                    }
                }
                pclose(p_pid);
            }
        } else {
            fclose(f_pid);
        }
    }

    char target_class[256];
    get_widget_class(widget_id, target_class, sizeof(target_class));
    
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "hyprctl clients -j | jq -r --arg class \"%s\" '.[] | select((.class // \"\") == $class or (.initialClass // \"\") == $class) | .address' | head -n1", target_class);
    
    FILE *p = popen(cmd, "r");
    if (p) {
        char buf[64];
        if (fgets(buf, sizeof(buf), p)) {
            trim(buf);
            snprintf(dest, dest_size, "%s", buf);
            pclose(p);
            return (dest[0] != '\0');
        }
        pclose(p);
    }
    return 0;
}

void dedupe_widget_windows(const char *widget_id) {
    char target_class[256];
    char keep_addr[64] = "";
    char cmd[1024];

    get_widget_class(widget_id, target_class, sizeof(target_class));
    get_widget_address(widget_id, keep_addr, sizeof(keep_addr));

    snprintf(cmd, sizeof(cmd),
             "hyprctl clients -j | jq -r --arg class \"%s\" --arg keep \"%s\" "
             "'.[] | select((.class // \"\") == $class or (.initialClass // \"\") == $class) | select(.address != $keep) | .address' | "
             "while read -r addr; do [ -n \"$addr\" ] && hyprctl dispatch closewindow \"address:$addr\" >/dev/null 2>&1; done",
             target_class, keep_addr);
    system(cmd);
}

int is_widget_running(const char *widget_id) {
    char addr[64];
    if (get_widget_address(widget_id, addr, sizeof(addr))) {
        return 1;
    }
    char pid_dir[512];
    get_runtime_path(pid_dir, sizeof(pid_dir), "");
    char fpath[1024];
    snprintf(fpath, sizeof(fpath), "%s/%s.pid", pid_dir, widget_id);
    FILE *f = fopen(fpath, "r");
    if (f) {
        int pid = 0;
        if (fscanf(f, "%d", &pid) == 1) {
            fclose(f);
            if (kill(pid, 0) == 0) return 1;
        } else {
            fclose(f);
        }
    }
    return 0;
}

void get_widget_meta_defaults(const char *widget_id, char *w, size_t w_sz, char *h, size_t h_sz, char *dx, size_t dx_sz, char *dy, size_t dy_sz) {
    int y = 72;
    int gap = 18;
    char gap_val[16] = "";
    char config_f[512];
    char order[4096] = "";
    char copy[4096];
    char *tok;
    char *saveptr = NULL;

    snprintf(w, w_sz, "520");
    snprintf(h, h_sz, "360");
    snprintf(dx, dx_sz, "40");
    snprintf(dy, dy_sz, "72");
    
    if (strcmp(widget_id, "clock") == 0) {
        snprintf(w, w_sz, "300");
        snprintf(h, h_sz, "140");
    } else if (strcmp(widget_id, "cava") == 0) {
        snprintf(w, w_sz, "500");
        snprintf(h, h_sz, "168");
    } else if (strcmp(widget_id, "system") == 0) {
        snprintf(w, w_sz, "380");
        snprintf(h, h_sz, "210");
    } else {
        get_custom_widget_meta(widget_id, "WIDTH", w, w_sz);
        get_custom_widget_meta(widget_id, "HEIGHT", h, h_sz);
    }

    get_env_path(config_f, sizeof(config_f), "anto426/widgets.env");
    if (read_env_value(config_f, "ANTO426_WIDGET_GAP", gap_val, sizeof(gap_val)) && atoi(gap_val) >= 0) {
        gap = atoi(gap_val);
    }

    get_widget_order_str(order, sizeof(order));
    snprintf(copy, sizeof(copy), "%s", order);
    tok = strtok_r(copy, " \t\r\n", &saveptr);
    while (tok) {
        char cur_w[16] = "520";
        char cur_h[16] = "360";

        if (strcmp(tok, widget_id) == 0) {
            snprintf(dy, dy_sz, "%d", y);
            return;
        }

        if (strcmp(tok, "clock") == 0) {
            snprintf(cur_h, sizeof(cur_h), "140");
        } else if (strcmp(tok, "cava") == 0) {
            snprintf(cur_h, sizeof(cur_h), "168");
        } else if (strcmp(tok, "system") == 0) {
            snprintf(cur_h, sizeof(cur_h), "210");
        } else {
            get_custom_widget_meta(tok, "WIDTH", cur_w, sizeof(cur_w));
            get_custom_widget_meta(tok, "HEIGHT", cur_h, sizeof(cur_h));
        }
        (void)cur_w;
        y += atoi(cur_h) + gap;
        tok = strtok_r(NULL, " \t\r\n", &saveptr);
    }

    if (strcmp(widget_id, "cava") == 0) {
        snprintf(dy, dy_sz, "230");
    } else if (strcmp(widget_id, "system") == 0) {
        snprintf(dy, dy_sz, "418");
    }
}

void escape_shell_cmd(const char *src, char *dest, size_t dest_sz) {
    size_t j = 0;
    for (size_t i = 0; src[i] != '\0' && j < dest_sz - 2; i++) {
        if (src[i] == '"' || src[i] == '\\' || src[i] == '$') {
            dest[j++] = '\\';
            dest[j++] = src[i];
        } else {
            dest[j++] = src[i];
        }
    }
    dest[j] = '\0';
}

int launch_terminal(const char *name, const char *wm_class, const char *title, const char *command) {
    char term[64];
    char pid_dir[512];
    char pid_file[1024];
    pid_t pid;

    if (!get_terminal(term, sizeof(term))) {
        notify("Compatible terminal not found");
        return 0;
    }

    get_runtime_path(pid_dir, sizeof(pid_dir), "");
    mkdir_p(pid_dir);
    snprintf(pid_file, sizeof(pid_file), "%s/%s.pid", pid_dir, name);

    pid = fork();
    if (pid < 0) {
        notify("Widget launch failed");
        return 0;
    }

    if (pid == 0) {
        int devnull = open("/dev/null", O_RDWR);
        char class_arg[512];
        char title_arg[512];
        char alacritty_class[512];
        const int compact = strcmp(name, "scheda_macchina") == 0;
        const char *font_size = compact ? "10" : "11";
        const char *padding_x = compact ? "12" : "16";
        const char *padding_y = compact ? "10" : "12";
        char font_size_arg[64];
        char padding_x_arg[64];
        char padding_y_arg[64];
        char kitty_font_override[64];
        char kitty_padding_override[64];

        setsid();
        if (devnull >= 0) {
            dup2(devnull, STDIN_FILENO);
            dup2(devnull, STDOUT_FILENO);
            dup2(devnull, STDERR_FILENO);
            if (devnull > STDERR_FILENO) close(devnull);
        }

        if (strcmp(term, "ghostty") == 0) {
            snprintf(class_arg, sizeof(class_arg), "--class=%s", wm_class);
            snprintf(title_arg, sizeof(title_arg), "--title=%s", title);
            snprintf(font_size_arg, sizeof(font_size_arg), "--font-size=%s", font_size);
            snprintf(padding_x_arg, sizeof(padding_x_arg), "--window-padding-x=%s", padding_x);
            snprintf(padding_y_arg, sizeof(padding_y_arg), "--window-padding-y=%s", padding_y);
            execlp(term, term, class_arg, title_arg, font_size_arg,
                   "--font-family=JetBrainsMono Nerd Font", padding_x_arg,
                   padding_y_arg, "--background-opacity=0.74",
                   "--window-decoration=false", "--confirm-close-surface=false",
                   "-e", "bash", "-lc", command, (char *)NULL);
        } else if (strcmp(term, "kitty") == 0) {
            snprintf(kitty_font_override, sizeof(kitty_font_override), "font_size=%s", font_size);
            snprintf(kitty_padding_override, sizeof(kitty_padding_override), "window_padding_width=%s", padding_x);
            execlp(term, term, "--class", wm_class, "--title", title,
                   "--override", kitty_font_override,
                   "--override", kitty_padding_override,
                   "--override", "background_opacity=0.74",
                   "--override", "hide_window_decorations=yes",
                   "bash", "-lc", command, (char *)NULL);
        } else if (strcmp(term, "foot") == 0) {
            execlp(term, term, "--app-id", wm_class, "--title", title,
                   "bash", "-lc", command, (char *)NULL);
        } else if (strcmp(term, "alacritty") == 0) {
            snprintf(alacritty_class, sizeof(alacritty_class), "%s,%s", wm_class, wm_class);
            execlp(term, term, "--class", alacritty_class, "--title", title,
                   "-e", "bash", "-lc", command, (char *)NULL);
        }
        _exit(127);
    }

    FILE *f = fopen(pid_file, "w");
    if (f) {
        fprintf(f, "%d\n", pid);
        fclose(f);
    }
    return 1;
}

void write_custom_widget_env(const char *widget_id, const char *name, const char *wm_class, const char *command, const char *mode, const char *w, const char *h, const char *monitor) {
    char dir[512];
    get_env_path(dir, sizeof(dir), "anto426/widgets.d");
    mkdir_p(dir);
    
    char fpath[1024];
    snprintf(fpath, sizeof(fpath), "%s/%s.env", dir, widget_id);
    FILE *f = fopen(fpath, "w");
    if (f) {
        write_env_quoted(f, "ANTO426_WIDGET_ID", widget_id, 0);
        write_env_quoted(f, "ANTO426_WIDGET_NAME", name, 0);
        write_env_quoted(f, "ANTO426_WIDGET_CLASS", wm_class, 0);
        write_env_quoted(f, "ANTO426_WIDGET_COMMAND", command, 0);
        write_env_quoted(f, "ANTO426_WIDGET_MODE", mode, 0);
        write_env_quoted(f, "ANTO426_WIDGET_WIDTH", w, 0);
        write_env_quoted(f, "ANTO426_WIDGET_HEIGHT", h, 0);
        write_env_quoted(f, "ANTO426_WIDGET_MONITOR", monitor, 0);
        fclose(f);
    }
}

int launch_custom_widget(const char *widget_id, int locked) {
    char mode[64] = "app";
    char wm_class[256] = "";
    char title[256] = "";
    char command[4096] = "";
    char monitor[64] = "";
    char w[16] = "520";
    char h[16] = "360";
    char x[16] = "40";
    char y[16] = "72";
    
    get_custom_widget_meta(widget_id, "MODE", mode, sizeof(mode));
    get_custom_widget_meta(widget_id, "CLASS", wm_class, sizeof(wm_class));
    get_custom_widget_meta(widget_id, "NAME", title, sizeof(title));
    get_custom_widget_meta(widget_id, "COMMAND", command, sizeof(command));
    get_custom_widget_meta(widget_id, "MONITOR", monitor, sizeof(monitor));
    get_custom_widget_meta(widget_id, "WIDTH", w, sizeof(w));
    get_custom_widget_meta(widget_id, "HEIGHT", h, sizeof(h));
    if (wm_class[0] == '\0') get_widget_class(widget_id, wm_class, sizeof(wm_class));
    if (title[0] == '\0') snprintf(title, sizeof(title), "%s", get_widget_label(widget_id));
    
    char val_x[16], val_y[16];
    if (read_layout_value(widget_id, "X", val_x, sizeof(val_x))) snprintf(x, sizeof(x), "%s", val_x);
    if (read_layout_value(widget_id, "Y", val_y, sizeof(val_y))) snprintf(y, sizeof(y), "%s", val_y);
    
    if (strcmp(mode, "terminal") == 0) {
        return launch_terminal(widget_id, wm_class, title, command);
    } else {
        char pid_dir[512];
        char cmd_file[1024];
        char runner_file[1024];
        get_runtime_path(pid_dir, sizeof(pid_dir), "");
        mkdir_p(pid_dir);
        
        char rules[4096];
        if (monitor[0] != '\0') {
            snprintf(rules, sizeof(rules), "monitor %s silent;float;size %s %s;move %s %s;rounding 14;opacity 0.94 0.88;border_size 1;no_anim", monitor, w, h, x, y);
        } else {
            snprintf(rules, sizeof(rules), "float;size %s %s;move %s %s;rounding 14;opacity 0.94 0.88;border_size 1;no_anim", w, h, x, y);
        }
        if (locked) {
            strncat(rules, ";no_focus;no_follow_mouse;no_initial_focus;focus_on_activate off", sizeof(rules) - strlen(rules) - 1);
        }

        snprintf(cmd_file, sizeof(cmd_file), "%s/%s.command", pid_dir, widget_id);
        snprintf(runner_file, sizeof(runner_file), "%s/%s.runner.sh", pid_dir, widget_id);

        FILE *fc = fopen(cmd_file, "w");
        if (fc) {
            fputs(command, fc);
            fclose(fc);
        }

        FILE *fr = fopen(runner_file, "w");
        if (fr) {
            fprintf(fr,
                    "#!/usr/bin/env bash\n"
                    "printf '%%s\\n' \"$$\" > \"%s/%s.pid\"\n"
                    "widget_command=\"$(cat \"%s\" 2>/dev/null)\"\n"
                    "exec bash -lc \"$widget_command\"\n",
                    pid_dir, widget_id, cmd_file);
            fclose(fr);
            chmod(runner_file, 0700);
        }
        
        char full_cmd[8192];
        snprintf(full_cmd, sizeof(full_cmd), "hyprctl dispatch exec \"[%s] bash %s\" >/dev/null 2>&1",
                 rules, runner_file);
        system(full_cmd);
        return 1;
    }
}

void stop_widget(const char *widget_id, const char *wm_class) {
    char pid_dir[512];
    get_runtime_path(pid_dir, sizeof(pid_dir), "");
    char fpath[1024];
    snprintf(fpath, sizeof(fpath), "%s/%s.pid", pid_dir, widget_id);
    
    FILE *f = fopen(fpath, "r");
    if (f) {
        int pid = 0;
        if (fscanf(f, "%d", &pid) == 1) {
            kill(pid, SIGTERM);
        }
        fclose(f);
        unlink(fpath);
    }
    
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "hyprctl dispatch closewindow \"class:%s\" >/dev/null 2>&1 || true", wm_class);
    system(cmd);
}

void stop_custom_widget(const char *widget_id) {
    char mode[64] = "app";
    char wm_class[256] = "";
    get_custom_widget_meta(widget_id, "MODE", mode, sizeof(mode));
    get_custom_widget_meta(widget_id, "CLASS", wm_class, sizeof(wm_class));
    
    if (strcmp(mode, "terminal") == 0) {
        stop_widget(widget_id, wm_class);
        return;
    }
    
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "hyprctl dispatch closewindow \"class:%s\" >/dev/null 2>&1", wm_class);
    system(cmd);
    
    char pid_dir[512];
    get_runtime_path(pid_dir, sizeof(pid_dir), "");
    char fpath[1024];
    snprintf(fpath, sizeof(fpath), "%s/%s.pid", pid_dir, widget_id);
    unlink(fpath);
}

void ensure_config() {
    char config_f[512];
    get_env_path(config_f, sizeof(config_f), "anto426/widgets.env");
    
    struct stat st;
    if (stat(config_f, &st) != 0) {
        char dir[512];
        get_env_path(dir, sizeof(dir), "anto426");
        mkdir_p(dir);
        
        FILE *f = fopen(config_f, "w");
        if (f) {
            fprintf(f, "# Set to 0 to prevent widgets from starting with Hyprland.\n"
                       "export ANTO426_WIDGETS_AUTOSTART=1\n\n"
                       "# Available widgets: clock cava system\n"
                       "export ANTO426_WIDGETS_ENABLED=\"clock cava system\"\n\n"
                       "# Ordine verticale predefinito (riordina con: widgets.sh arrange)\n"
                       "export ANTO426_WIDGET_ORDER=\"clock cava system\"\n\n"
                       "# Spazio tra widget quando si resetta il layout\n"
                       "export ANTO426_WIDGET_GAP=18\n\n"
                       "# Terminale usato per i widget (ghostty consigliato)\n"
                       "export ANTO426_WIDGETS_BACKEND=\"terminal\"\n\n"
                       "# 1 = widget bloccati: non prendono focus e non si spostano per sbaglio.\n"
                       "export ANTO426_WIDGETS_LOCKED=0\n");
            fclose(f);
        }
    }
}

void update_cava_colors() {
    char cava_config[512];
    get_env_path(cava_config, sizeof(cava_config), "anto426/cava_widget.conf");
    char colors_file[512];
    get_env_path(colors_file, sizeof(colors_file), "colors/colors.sh");
    
    struct stat st1, st2;
    if (stat(cava_config, &st1) == 0 && stat(colors_file, &st2) == 0) {
        char accent[64] = "#9579b6";
        char fg[64] = "#f6f7fb";
        read_env_value(colors_file, "ANTO426_ACCENT", accent, sizeof(accent));
        read_env_value(colors_file, "ANTO426_FOREGROUND", fg, sizeof(fg));
        
        char sed_cmd[2048];
        snprintf(sed_cmd, sizeof(sed_cmd), "sed -i -e \"s|^gradient_1 = .*|gradient_1 = '%s'|\" -e \"s|^gradient_2 = .*|gradient_2 = '%s'|\" %s >/dev/null 2>&1",
                 accent, fg, cava_config);
        system(sed_cmd);
    }
}

void write_hypr_rules(int locked) {
    char lock_f[512];
    get_env_path(lock_f, sizeof(lock_f), "hypr/conf/widget-lock.generated.conf");
    char apps_f[512];
    get_env_path(apps_f, sizeof(apps_f), "hypr/conf/widget-apps.generated.conf");
    
    char dir[512];
    get_env_path(dir, sizeof(dir), "hypr/conf");
    mkdir_p(dir);
    
    char config_f[512];
    get_env_path(config_f, sizeof(config_f), "anto426/widgets.env");
    char enabled_str[1024] = "";
    get_enabled_widget_ids_str(enabled_str, sizeof(enabled_str));
    
    char layout_f[512];
    get_env_path(layout_f, sizeof(layout_f), "anto426/widgets_layout.env");
    
    char customs_str[2048] = "";
    get_custom_widget_ids_str(customs_str, sizeof(customs_str));
    
    FILE *fl = fopen(lock_f, "w");
    if (fl) {
        fprintf(fl, "# Generated by widgets_core - lock state rules\n");
        if (locked) {
            char *enabled_copy = strdup(enabled_str);
            char *saveptr_enabled = NULL;
            char *tok = strtok_r(enabled_copy, " \t\r\n", &saveptr_enabled);
            while (tok) {
                char w[16], h[16], dx[16], dy[16];
                get_widget_meta_defaults(tok, w, sizeof(w), h, sizeof(h), dx, sizeof(dx), dy, sizeof(dy));
                
                char key_x[64], key_y[64], key_w[64], key_h[64];
                snprintf(key_x, sizeof(key_x), "LAYOUT_%s_x", tok);
                snprintf(key_y, sizeof(key_y), "LAYOUT_%s_y", tok);
                snprintf(key_w, sizeof(key_w), "LAYOUT_%s_w", tok);
                snprintf(key_h, sizeof(key_h), "LAYOUT_%s_h", tok);
                for (int i=0; key_x[i]; i++) { key_x[i]=toupper((unsigned char)key_x[i]); key_y[i]=toupper((unsigned char)key_y[i]); key_w[i]=toupper((unsigned char)key_w[i]); key_h[i]=toupper((unsigned char)key_h[i]); }
                
                char val_x[16], val_y[16], val_w[16], val_h[16];
                if (read_env_value(layout_f, key_x, val_x, sizeof(val_x))) strcpy(dx, val_x);
                if (read_env_value(layout_f, key_y, val_y, sizeof(val_y))) strcpy(dy, val_y);
                if (read_env_value(layout_f, key_w, val_w, sizeof(val_w))) strcpy(w, val_w);
                if (read_env_value(layout_f, key_h, val_h, sizeof(val_h))) strcpy(h, val_h);
                
                char wm_class[256];
                char wm_regex[512];
                get_widget_class(tok, wm_class, sizeof(wm_class));
                regex_escape(wm_class, wm_regex, sizeof(wm_regex));
                
                fprintf(fl, "windowrule = match:class ^%s$, float on\n", wm_regex);
                fprintf(fl, "windowrule = match:class ^%s$, size %s %s\n", wm_regex, w, h);
                fprintf(fl, "windowrule = match:class ^%s$, move %s %s\n", wm_regex, dx, dy);
                fprintf(fl, "windowrule = match:class ^%s$, no_focus on\n", wm_regex);
                fprintf(fl, "windowrule = match:class ^%s$, no_follow_mouse on\n", wm_regex);
                fprintf(fl, "windowrule = match:class ^%s$, no_initial_focus on\n", wm_regex);
                fprintf(fl, "windowrule = match:class ^%s$, focus_on_activate off\n", wm_regex);
                fprintf(fl, "windowrule = match:class ^%s$, suppress_event activate activatefocus maximize fullscreen\n", wm_regex);
                fprintf(fl, "windowrule = match:class ^%s$, border_size 0\n", wm_regex);
                fprintf(fl, "windowrule = match:class ^%s$, dim_around off\n", wm_regex);
                
                tok = strtok_r(NULL, " \t\r\n", &saveptr_enabled);
            }
            free(enabled_copy);
            
            char *customs_copy = strdup(customs_str);
            char *saveptr_customs = NULL;
            tok = strtok_r(customs_copy, " \t\r\n", &saveptr_customs);
            while (tok) {
                char mode[64] = "app";
                get_custom_widget_meta(tok, "MODE", mode, sizeof(mode));
                if (strcmp(mode, "terminal") == 0) {
                    char wm_class[256] = "";
                    char wm_regex[512];
                    get_widget_class(tok, wm_class, sizeof(wm_class));
                    regex_escape(wm_class, wm_regex, sizeof(wm_regex));
                    char w[16], h[16], dx[16], dy[16];
                    get_widget_meta_defaults(tok, w, sizeof(w), h, sizeof(h), dx, sizeof(dx), dy, sizeof(dy));
                    
                    char key_x[64], key_y[64], key_w[64], key_h[64];
                    snprintf(key_x, sizeof(key_x), "LAYOUT_%s_x", tok);
                    snprintf(key_y, sizeof(key_y), "LAYOUT_%s_y", tok);
                    snprintf(key_w, sizeof(key_w), "LAYOUT_%s_w", tok);
                    snprintf(key_h, sizeof(key_h), "LAYOUT_%s_h", tok);
                    for (int i=0; key_x[i]; i++) { key_x[i]=toupper((unsigned char)key_x[i]); key_y[i]=toupper((unsigned char)key_y[i]); key_w[i]=toupper((unsigned char)key_w[i]); key_h[i]=toupper((unsigned char)key_h[i]); }
                    
                    char val_x[16], val_y[16], val_w[16], val_h[16];
                    if (read_env_value(layout_f, key_x, val_x, sizeof(val_x))) strcpy(dx, val_x);
                    if (read_env_value(layout_f, key_y, val_y, sizeof(val_y))) strcpy(dy, val_y);
                    if (read_env_value(layout_f, key_w, val_w, sizeof(val_w))) strcpy(w, val_w);
                    if (read_env_value(layout_f, key_h, val_h, sizeof(val_h))) strcpy(h, val_h);
                    
                    fprintf(fl, "windowrule = match:class ^%s$, float on\n", wm_regex);
                    fprintf(fl, "windowrule = match:class ^%s$, size %s %s\n", wm_regex, w, h);
                    fprintf(fl, "windowrule = match:class ^%s$, move %s %s\n", wm_regex, dx, dy);
                    fprintf(fl, "windowrule = match:class ^%s$, no_focus on\n", wm_regex);
                    fprintf(fl, "windowrule = match:class ^%s$, no_follow_mouse on\n", wm_regex);
                    fprintf(fl, "windowrule = match:class ^%s$, no_initial_focus on\n", wm_regex);
                    fprintf(fl, "windowrule = match:class ^%s$, focus_on_activate off\n", wm_regex);
                    fprintf(fl, "windowrule = match:class ^%s$, suppress_event activate activatefocus maximize fullscreen\n", wm_regex);
                    fprintf(fl, "windowrule = match:class ^%s$, border_size 0\n", wm_regex);
                    fprintf(fl, "windowrule = match:class ^%s$, dim_around off\n", wm_regex);
                }
                tok = strtok_r(NULL, " \t\r\n", &saveptr_customs);
            }
            free(customs_copy);
        }
        fclose(fl);
    }
    
    FILE *fa = fopen(apps_f, "w");
    if (fa) {
        fprintf(fa, "# Generated by widgets_core - custom widget window rules\n");
        char *customs_copy = strdup(customs_str);
        char *saveptr_customs = NULL;
        char *tok = strtok_r(customs_copy, " \t\r\n", &saveptr_customs);
        while (tok) {
            char mode[64] = "app";
            get_custom_widget_meta(tok, "MODE", mode, sizeof(mode));
            if (strcmp(mode, "terminal") == 0) {
                char wm_class[256] = "";
                char wm_regex[512];
                get_widget_class(tok, wm_class, sizeof(wm_class));
                regex_escape(wm_class, wm_regex, sizeof(wm_regex));
                char w[16], h[16], dx[16], dy[16];
                get_widget_meta_defaults(tok, w, sizeof(w), h, sizeof(h), dx, sizeof(dx), dy, sizeof(dy));
                char monitor[64] = "";
                get_custom_widget_meta(tok, "MONITOR", monitor, sizeof(monitor));
                
                char key_x[64], key_y[64], key_w[64], key_h[64];
                snprintf(key_x, sizeof(key_x), "LAYOUT_%s_x", tok);
                snprintf(key_y, sizeof(key_y), "LAYOUT_%s_y", tok);
                snprintf(key_w, sizeof(key_w), "LAYOUT_%s_w", tok);
                snprintf(key_h, sizeof(key_h), "LAYOUT_%s_h", tok);
                for (int i=0; key_x[i]; i++) { key_x[i]=toupper((unsigned char)key_x[i]); key_y[i]=toupper((unsigned char)key_y[i]); key_w[i]=toupper((unsigned char)key_w[i]); key_h[i]=toupper((unsigned char)key_h[i]); }
                
                char val_x[16], val_y[16], val_w[16], val_h[16];
                if (read_env_value(layout_f, key_x, val_x, sizeof(val_x))) strcpy(dx, val_x);
                if (read_env_value(layout_f, key_y, val_y, sizeof(val_y))) strcpy(dy, val_y);
                if (read_env_value(layout_f, key_w, val_w, sizeof(val_w))) strcpy(w, val_w);
                if (read_env_value(layout_f, key_h, val_h, sizeof(val_h))) strcpy(h, val_h);
                
                fprintf(fa, "windowrule = float on, match:class ^%s$\n", wm_regex);
                if (monitor[0] != '\0') {
                    fprintf(fa, "windowrule = monitor %s silent, match:class ^%s$\n", monitor, wm_regex);
                }
                fprintf(fa, "windowrule = size %s %s, match:class ^%s$\n", w, h, wm_regex);
                fprintf(fa, "windowrule = move %s %s, match:class ^%s$\n", dx, dy, wm_regex);
                fprintf(fa, "windowrule = border_size 1, match:class ^%s$\n", wm_regex);
                fprintf(fa, "windowrule = no_shadow on, match:class ^%s$\n", wm_regex);
                fprintf(fa, "windowrule = no_anim on, match:class ^%s$\n", wm_regex);
                fprintf(fa, "windowrule = no_initial_focus on, match:class ^%s$\n", wm_regex);
                fprintf(fa, "windowrule = no_follow_mouse on, match:class ^%s$\n", wm_regex);
                fprintf(fa, "windowrule = focus_on_activate off, match:class ^%s$\n", wm_regex);
                fprintf(fa, "windowrule = rounding 14, match:class ^%s$\n", wm_regex);
                fprintf(fa, "windowrule = opacity 0.94 0.88, match:class ^%s$\n", wm_regex);
            }
            tok = strtok_r(NULL, " \t\r\n", &saveptr_customs);
        }
        free(customs_copy);
        fclose(fa);
    }
}

const char *get_clock_command() {
    static char cmd[8192];
    snprintf(cmd, sizeof(cmd),
             "source ~/.config/colors/colors.sh 2>/dev/null || true; "
             "printf \"\\033[?25l\"; "
             "trap \"printf \\\"\\033[?25h\\\"\" EXIT; "
             "while true; do "
             "  clear; "
             "  printf \"\\n\"; "
             "  printf \"  \\033[1m%%s\\033[0m\\n\" \"$(date +%%H:%%M)\"; "
             "  printf \"  %%s\\n\" \"$(date \"+%%A %%d %%B\")\"; "
             "  printf \"  \\033[2m%%s\\033[0m\\n\" \"$(date \"+%%Y\")\"; "
             "  sleep 1; "
             "done");
    return cmd;
}

const char *get_cava_command() {
    static char cmd[8192];
    char cava_conf[1024];
    get_env_path(cava_conf, sizeof(cava_conf), "anto426/cava_widget.conf");
    snprintf(cmd, sizeof(cmd),
             "source ~/.config/colors/colors.sh 2>/dev/null || true; "
             "printf \"\\033[?25l\"; "
             "trap \"printf \\\"\\033[?25h\\\"\" EXIT; "
             "if command -v cava >/dev/null 2>&1; then "
             "  exec cava -p \"%s\"; "
             "else "
             "  while true; do "
             "    clear; "
             "    printf \"\\n  󰎈  Audio visualizer\\n\\n\"; "
             "    printf \"  Installa cava per il visualizzatore\\n\"; "
             "    sleep 5; "
             "  done; "
             "fi", cava_conf);
    return cmd;
}

const char *get_system_command() {
    static char cmd[8192];
    snprintf(cmd, sizeof(cmd),
             "source ~/.config/colors/colors.sh 2>/dev/null || true; "
             "printf \"\\033[?25l\"; "
             "trap \"printf \\\"\\033[?25h\\\"\" EXIT; "
             "bar() { "
             "  value=\"${1:-0}\"; "
             "  filled=$((value / 10)); "
             "  out=\"\"; "
             "  i=0; "
             "  while [ \"$i\" -lt 10 ]; do "
             "    if [ \"$i\" -lt \"$filled\" ]; then out=\"${out}█\"; else out=\"${out}░\"; fi; "
             "    i=$((i + 1)); "
             "  done; "
             "  printf \"%%s\" \"$out\"; "
             "}; "
             "pct() { "
             "  wpctl get-volume \"$1\" 2>/dev/null | awk \'/^Volume:/ { v=int(($2*100)+0.5); if(v<0)v=0; if(v>100)v=100; print v; exit }\'; "
             "}; "
             "while true; do "
             "  clear; "
             "  battery=\"n/d\"; "
             "  for b in /sys/class/power_supply/BAT*; do "
             "    [ -r \"$b/capacity\" ] && battery=\"$(cat \"$b/capacity\")%%\" && break; "
             "  done; "
             "  mem=\"$(free -h 2>/dev/null | awk \'/^Mem:/ {print $3 \" / \" $2}\')\"; "
             "  load=\"$(cut -d\" \" -f1-3 /proc/loadavg 2>/dev/null)\"; "
             "  vol=\"$(pct @DEFAULT_AUDIO_SINK@)\"; "
             "  mic=\"$(pct @DEFAULT_AUDIO_SOURCE@)\"; "
             "  brightness=\"$(brightnessctl -m 2>/dev/null | awk -F, \'{gsub(/%%/,\"\",$4); print int($4); exit}\')\"; "
             "  temp=\"$(sensors 2>/dev/null | awk \'/Package id 0|Tctl|Tdie/ {print $3; exit}\')\"; "
             "  printf \"\\n  󰍛  Sistema\\n\\n\"; "
             "  printf \"  󰁹  Batteria   %%4s  %%s\\n\" \"$battery\" \"$(bar \"${battery%%%%%%}\")\"; "
             "  printf \"  󰍛  RAM        %%s\\n\" \"${mem:-n/d}\"; "
             "  printf \"  󰻠  CPU        %%s\\n\" \"${load:-n/d}\"; "
             "  [[ -n \"$temp\" ]] && printf \"  󰈸  Temp       %%s\\n\" \"$temp\"; "
             "  printf \"  󰕾  Volume     %%3s%%%%  %%s\\n\" \"${vol:-?}\" \"$(bar \"${vol:-0}\")\"; "
             "  printf \"  󰍬  Mic        %%3s%%%%  %%s\\n\" \"${mic:-?}\" \"$(bar \"${mic:-0}\")\"; "
             "  [[ -n \"$brightness\" ]] && printf \"  󰃠  Luce       %%3s%%%%  %%s\\n\" \"$brightness\" \"$(bar \"$brightness\")\"; "
             "  sleep 2; "
             "done");
    return cmd;
}

void apply_widget_layout_single(const char *widget_id, const char *monitor, int active_ws, int locked, const char *addr_arg) {
    char addr[64];
    if (addr_arg && addr_arg[0] != '\0') {
        snprintf(addr, sizeof(addr), "%s", addr_arg);
    } else {
        if (!get_widget_address(widget_id, addr, sizeof(addr))) {
            return;
        }
    }
    
    char ws_name[128];
    snprintf(ws_name, sizeof(ws_name), "%d", active_ws);
    
    char cmd_ws[1024];
    snprintf(cmd_ws, sizeof(cmd_ws), "hyprctl monitors -j | jq -r --arg name \"%s\" '.[] | select(.name == $name) | .activeWorkspace.name' | head -n1", monitor);
    FILE *p = popen(cmd_ws, "r");
    if (p) {
        char buf[128];
        if (fgets(buf, sizeof(buf), p)) {
            trim(buf);
            if (buf[0] != '\0') snprintf(ws_name, sizeof(ws_name), "%s", buf);
        }
        pclose(p);
    }
    
    char ws_arg[256];
    int is_num = 1;
    for (int i=0; ws_name[i]; i++) {
        if (!isdigit((unsigned char)ws_name[i])) { is_num = 0; break; }
    }
    if (!is_num && strncmp(ws_name, "special:", 8) != 0) {
        snprintf(ws_arg, sizeof(ws_arg), "name:%s", ws_name);
    } else {
        snprintf(ws_arg, sizeof(ws_arg), "%s", ws_name);
    }
    
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "hyprctl dispatch movetoworkspacesilent \"%s,address:%s\" >/dev/null 2>&1", ws_arg, addr);
    system(cmd);
    
    char layout_f[512];
    get_env_path(layout_f, sizeof(layout_f), "anto426/widgets_layout.env");
    
    char key_x[64], key_y[64], key_w[64], key_h[64];
    snprintf(key_x, sizeof(key_x), "LAYOUT_%s_x", widget_id);
    snprintf(key_y, sizeof(key_y), "LAYOUT_%s_y", widget_id);
    snprintf(key_w, sizeof(key_w), "LAYOUT_%s_w", widget_id);
    snprintf(key_h, sizeof(key_h), "LAYOUT_%s_h", widget_id);
    for (int i=0; key_x[i]; i++) { key_x[i]=toupper((unsigned char)key_x[i]); key_y[i]=toupper((unsigned char)key_y[i]); key_w[i]=toupper((unsigned char)key_w[i]); key_h[i]=toupper((unsigned char)key_h[i]); }
    
    char w[16], h[16], x[16], y[16];
    get_widget_meta_defaults(widget_id, w, sizeof(w), h, sizeof(h), x, sizeof(x), y, sizeof(y));
    
    char val_x[16], val_y[16], val_w[16], val_h[16];
    if (read_env_value(layout_f, key_x, val_x, sizeof(val_x))) strcpy(x, val_x);
    if (read_env_value(layout_f, key_y, val_y, sizeof(val_y))) strcpy(y, val_y);
    if (read_env_value(layout_f, key_w, val_w, sizeof(val_w))) strcpy(w, val_w);
    if (read_env_value(layout_f, key_h, val_h, sizeof(val_h))) strcpy(h, val_h);
    
    snprintf(cmd, sizeof(cmd), "hyprctl dispatch movewindowpixel exact %s %s,address:%s >/dev/null 2>&1", x, y, addr);
    system(cmd);
    snprintf(cmd, sizeof(cmd), "hyprctl dispatch resizewindowpixel exact %s %s,address:%s >/dev/null 2>&1", w, h, addr);
    system(cmd);
    
    const char *zorder = locked ? "bottom" : "top";
    snprintf(cmd, sizeof(cmd), "hyprctl dispatch alterzorder %s,address:%s >/dev/null 2>&1", zorder, addr);
    system(cmd);
    
    const char *val = locked ? "1" : "0";
    snprintf(cmd, sizeof(cmd), "hyprctl setprop address:%s no_focus %s >/dev/null 2>&1", addr, val);
    system(cmd);
    snprintf(cmd, sizeof(cmd), "hyprctl setprop address:%s nofocus %s >/dev/null 2>&1", addr, val);
    system(cmd);
    snprintf(cmd, sizeof(cmd), "hyprctl setprop address:%s no_follow_mouse %s >/dev/null 2>&1", addr, val);
    system(cmd);
    snprintf(cmd, sizeof(cmd), "hyprctl setprop address:%s nofollowmouse %s >/dev/null 2>&1", addr, val);
    system(cmd);
    snprintf(cmd, sizeof(cmd), "hyprctl setprop address:%s focus_on_activate 0 >/dev/null 2>&1", addr);
    system(cmd);
}

void apply_widget_layouts() {
    char config_f[512];
    get_env_path(config_f, sizeof(config_f), "anto426/widgets.env");
    
    char order_str[4096] = "";
    get_widget_order_str(order_str, sizeof(order_str));
    
    int locked = 0;
    char locked_val[16] = "0";
    if (read_env_value(config_f, "ANTO426_WIDGETS_LOCKED", locked_val, sizeof(locked_val))) {
        locked = (strcmp(locked_val, "1") == 0);
    }
    
    char focused_monitor[64] = "eDP-1";
    FILE *p_mon = popen("hyprctl monitors -j | jq -r '.[] | select(.focused == true) | .name' | head -n1", "r");
    if (p_mon) {
        char buf[64];
        if (fgets(buf, sizeof(buf), p_mon)) {
            trim(buf);
            if (buf[0] != '\0') strcpy(focused_monitor, buf);
        }
        pclose(p_mon);
    }
    
    char mon_names[16][64];
    int mon_workspaces[16];
    int total_monitors = 0;
    
    p_mon = popen("hyprctl monitors -j | jq -r '.[] | \"\\(.name) \\(.activeWorkspace.id)\"'", "r");
    if (p_mon) {
        char line[512];
        while (fgets(line, sizeof(line), p_mon) && total_monitors < 16) {
            trim(line);
            char m_name[64] = "";
            int m_ws = 1;
            if (sscanf(line, "%63s %d", m_name, &m_ws) >= 2) {
                strcpy(mon_names[total_monitors], m_name);
                mon_workspaces[total_monitors] = m_ws;
                total_monitors++;
            }
        }
        pclose(p_mon);
    }
    
    char *order_copy = strdup(order_str);
    char *saveptr_order = NULL;
    char *tok = strtok_r(order_copy, " \t\r\n", &saveptr_order);
    while (tok) {
        char assigned_monitor[64] = "";
        if (!read_layout_value(tok, "MONITOR", assigned_monitor, sizeof(assigned_monitor))) {
            get_custom_widget_meta(tok, "MONITOR", assigned_monitor, sizeof(assigned_monitor));
        }
        if (assigned_monitor[0] == '\0') {
            strcpy(assigned_monitor, focused_monitor);
        }
        
        int active_ws = 1;
        for (int m = 0; m < total_monitors; m++) {
            if (strcmp(mon_names[m], assigned_monitor) == 0) {
                active_ws = mon_workspaces[m];
                break;
            }
        }
        
        apply_widget_layout_single(tok, assigned_monitor, active_ws, locked, NULL);
        tok = strtok_r(NULL, " \t\r\n", &saveptr_order);
    }
    free(order_copy);
}

void show_all_widgets_unlocked(void) {
    char ws_arg[256] = "1";
    FILE *p_ws = popen("hyprctl activeworkspace -j | jq -r 'if ((.name // \"\") | length) > 0 then if ((.name | test(\"^[0-9]+$\")) or (.name | startswith(\"special:\"))) then .name else \"name:\\(.name)\" end else (.id | tostring) end' | head -n1", "r");
    if (p_ws) {
        char buf[256];
        if (fgets(buf, sizeof(buf), p_ws)) {
            trim(buf);
            if (buf[0] != '\0') snprintf(ws_arg, sizeof(ws_arg), "%s", buf);
        }
        pclose(p_ws);
    }

    char known_str[4096] = "";
    get_known_widget_ids_str(known_str, sizeof(known_str));
    char *known_copy = strdup(known_str);
    char *saveptr = NULL;
    char *tok = strtok_r(known_copy, " \t\r\n", &saveptr);
    while (tok) {
        char addr[64];
        if (get_widget_address(tok, addr, sizeof(addr))) {
            char cmd[1024];
            snprintf(cmd, sizeof(cmd), "hyprctl dispatch movetoworkspacesilent \"%s,address:%s\" >/dev/null 2>&1", ws_arg, addr);
            system(cmd);
            snprintf(cmd, sizeof(cmd), "hyprctl dispatch alterzorder top,address:%s >/dev/null 2>&1", addr);
            system(cmd);
            snprintf(cmd, sizeof(cmd), "hyprctl setprop address:%s no_focus 0 >/dev/null 2>&1", addr);
            system(cmd);
            snprintf(cmd, sizeof(cmd), "hyprctl setprop address:%s nofocus 0 >/dev/null 2>&1", addr);
            system(cmd);
            snprintf(cmd, sizeof(cmd), "hyprctl setprop address:%s no_follow_mouse 0 >/dev/null 2>&1", addr);
            system(cmd);
            snprintf(cmd, sizeof(cmd), "hyprctl setprop address:%s nofollowmouse 0 >/dev/null 2>&1", addr);
            system(cmd);
            snprintf(cmd, sizeof(cmd), "hyprctl setprop address:%s focus_on_activate 0 >/dev/null 2>&1", addr);
            system(cmd);
        }
        tok = strtok_r(NULL, " \t\r\n", &saveptr);
    }
    free(known_copy);
}

int save_widget_layouts() {
    char config_f[512];
    get_env_path(config_f, sizeof(config_f), "anto426/widgets.env");
    char order_str[4096] = "";
    char saved_order[4096] = "";
    get_widget_order_str(order_str, sizeof(order_str));
    
    char layout_f[512];
    get_env_path(layout_f, sizeof(layout_f), "anto426/widgets_layout.env");
    
    FILE *f_lay = fopen(layout_f, "r");
    char *keys[256];
    char *vals[256];
    int lay_count = 0;
    if (f_lay) {
        char line[MAX_LINE];
        while (fgets(line, sizeof(line), f_lay)) {
            char *ptr = line;
            while (isspace((unsigned char)*ptr)) ptr++;
            if (*ptr == '\0' || *ptr == '#') continue;
            if (strncmp(ptr, "export ", 7) == 0) ptr += 7;
            char *eq = strchr(ptr, '=');
            if (eq) {
                *eq = '\0';
                char key[MAX_LINE], val[MAX_VAL];
                snprintf(key, sizeof(key), "%s", ptr);
                snprintf(val, sizeof(val), "%s", eq+1);
                trim(key); trim(val);
                remove_quotes(val);
                if (strcmp(key, "ANTO426_WIDGET_ORDER") != 0 && lay_count < 256) {
                    keys[lay_count] = strdup(key);
                    vals[lay_count] = strdup(val);
                    lay_count++;
                }
            }
        }
        fclose(f_lay);
    }
    
    FILE *p_clients = popen("hyprctl clients -j", "r");
    if (p_clients) {
        char *all_ids_copy = strdup(order_str);
        char *saveptr_ids = NULL;
        char *tok = strtok_r(all_ids_copy, " \t\r\n", &saveptr_ids);
        while (tok) {
            char wm_class[256];
            get_widget_class(tok, wm_class, sizeof(wm_class));
            
            char q_geom[1024];
            snprintf(q_geom, sizeof(q_geom), "hyprctl clients -j | jq -r --arg class \"%s\" '.[] | select((.class // \"\") == $class or (.initialClass // \"\") == $class) | \"\\(.at[0]) \\(.at[1]) \\(.size[0]) \\(.size[1]) \\(.monitor)\"' | head -n1", wm_class);
            FILE *p_g = popen(q_geom, "r");
            if (p_g) {
                char buf[512];
                if (fgets(buf, sizeof(buf), p_g)) {
                    trim(buf);
                    int cur_x = 0, cur_y = 0, cur_w = 0, cur_h = 0;
                    char cur_mon[64] = "";
                    if (sscanf(buf, "%d %d %d %d %63s", &cur_x, &cur_y, &cur_w, &cur_h, cur_mon) >= 4) {
                        int is_num = 1;
                        for (int i=0; cur_mon[i]; i++) {
                            if (!isdigit((unsigned char)cur_mon[i])) { is_num=0; break; }
                        }
                        if (is_num && cur_mon[0] != '\0') {
                            char q_mon[1024];
                            snprintf(q_mon, sizeof(q_mon), "hyprctl monitors -j | jq -r --arg id \"%s\" '.[] | select(.id == ($id | tonumber)) | .name' | head -n1", cur_mon);
                            FILE *p_m = popen(q_mon, "r");
                            if (p_m) {
                                char m_buf[64];
                                if (fgets(m_buf, sizeof(m_buf), p_m)) {
                                    trim(m_buf);
                                    if (m_buf[0] != '\0') strcpy(cur_mon, m_buf);
                                }
                                pclose(p_m);
                            }
                        }
                        
                        char k_x[64], k_y[64], k_w[64], k_h[64], k_m[64];
                        snprintf(k_x, sizeof(k_x), "LAYOUT_%s_X", tok);
                        snprintf(k_y, sizeof(k_y), "LAYOUT_%s_Y", tok);
                        snprintf(k_w, sizeof(k_w), "LAYOUT_%s_W", tok);
                        snprintf(k_h, sizeof(k_h), "LAYOUT_%s_H", tok);
                        snprintf(k_m, sizeof(k_m), "LAYOUT_%s_MONITOR", tok);
                        for (int k=0; k_x[k]; k++) { k_x[k]=toupper((unsigned char)k_x[k]); k_y[k]=toupper((unsigned char)k_y[k]); k_w[k]=toupper((unsigned char)k_w[k]); k_h[k]=toupper((unsigned char)k_h[k]); k_m[k]=toupper((unsigned char)k_m[k]); }
                        
                        char str_x[16], str_y[16], str_w[16], str_h[16];
                        snprintf(str_x, sizeof(str_x), "%d", cur_x);
                        snprintf(str_y, sizeof(str_y), "%d", cur_y);
                        snprintf(str_w, sizeof(str_w), "%d", cur_w);
                        snprintf(str_h, sizeof(str_h), "%d", cur_h);
                        
                        int f_x=0, f_y=0, f_w=0, f_h=0, f_m=0;
                        for (int j=0; j<lay_count; j++) {
                            if (strcmp(keys[j], k_x) == 0) { free(vals[j]); vals[j]=strdup(str_x); f_x=1; }
                            else if (strcmp(keys[j], k_y) == 0) { free(vals[j]); vals[j]=strdup(str_y); f_y=1; }
                            else if (strcmp(keys[j], k_w) == 0) { free(vals[j]); vals[j]=strdup(str_w); f_w=1; }
                            else if (strcmp(keys[j], k_h) == 0) { free(vals[j]); vals[j]=strdup(str_h); f_h=1; }
                            else if (strcmp(keys[j], k_m) == 0) { free(vals[j]); vals[j]=strdup(cur_mon); f_m=1; }
                        }
                        
                        if (!f_x && lay_count < 256) { keys[lay_count]=strdup(k_x); vals[lay_count]=strdup(str_x); lay_count++; }
                        if (!f_y && lay_count < 256) { keys[lay_count]=strdup(k_y); vals[lay_count]=strdup(str_y); lay_count++; }
                        if (!f_w && lay_count < 256) { keys[lay_count]=strdup(k_w); vals[lay_count]=strdup(str_w); lay_count++; }
                        if (!f_h && lay_count < 256) { keys[lay_count]=strdup(k_h); vals[lay_count]=strdup(str_h); lay_count++; }
                        if (!f_m && cur_mon[0] != '\0' && lay_count < 256) { keys[lay_count]=strdup(k_m); vals[lay_count]=strdup(cur_mon); lay_count++; }
                        append_unique_token(saved_order, sizeof(saved_order), tok);
                    }
                }
                pclose(p_g);
            }
            tok = strtok_r(NULL, " \t\r\n", &saveptr_ids);
        }
        free(all_ids_copy);
        pclose(p_clients);
    }
    
    if (saved_order[0] != '\0') {
        snprintf(order_str, sizeof(order_str), "%s", saved_order);
    }
    
    FILE *f_w = fopen(layout_f, "w");
    if (f_w) {
        fprintf(f_w, "# Posizioni widget (aggiornate con: widgets.sh save-layout)\n"
                     "# Riordina con: widgets.sh arrange\n"
                     "export ANTO426_WIDGET_ORDER=\"%s\"\n", order_str);
        for (int i=0; i<lay_count; i++) {
            fprintf(f_w, "export %s=\"%s\"\n", keys[i], vals[i]);
            free(keys[i]); free(vals[i]);
        }
        fclose(f_w);
    }
    
    return 1;
}

void start_widgets() {
    ensure_config();
    char config_f[512];
    get_env_path(config_f, sizeof(config_f), "anto426/widgets.env");
    
    char enabled_str[1024] = "";
    get_enabled_widget_ids_str(enabled_str, sizeof(enabled_str));
    
    char customs_str[2048] = "";
    get_custom_widget_ids_str(customs_str, sizeof(customs_str));
    
    int locked = 0;
    char locked_val[16] = "0";
    if (read_env_value(config_f, "ANTO426_WIDGETS_LOCKED", locked_val, sizeof(locked_val))) {
        locked = (strcmp(locked_val, "1") == 0);
    }
    
    write_hypr_rules(locked);
    update_cava_colors();
    
    char *enabled_copy = strdup(enabled_str);
    char *saveptr_enabled = NULL;
    char *tok = strtok_r(enabled_copy, " \t\r\n", &saveptr_enabled);
    while (tok) {
        if (!is_widget_running(tok)) {
            if (strcmp(tok, "clock") == 0) {
                launch_terminal("clock", "anto426.widget.clock", "Clock Widget", get_clock_command());
            } else if (strcmp(tok, "cava") == 0) {
                if (is_music_active()) {
                    launch_terminal("cava", "anto426.widget.cava", "Cava Widget", get_cava_command());
                }
            } else if (strcmp(tok, "system") == 0) {
                launch_terminal("system", "anto426.widget.system", "System Widget", get_system_command());
            }
        }
        tok = strtok_r(NULL, " \t\r\n", &saveptr_enabled);
    }
    free(enabled_copy);
    
    char *customs_copy = strdup(customs_str);
    char *saveptr_customs = NULL;
    tok = strtok_r(customs_copy, " \t\r\n", &saveptr_customs);
    while (tok) {
        if (!is_widget_running(tok)) {
            if (strcmp(tok, "spettro_audio") == 0) {
                if (is_music_active()) {
                    launch_custom_widget(tok, locked);
                }
            } else {
                launch_custom_widget(tok, locked);
            }
        }
        tok = strtok_r(NULL, " \t\r\n", &saveptr_customs);
    }
    free(customs_copy);
    
    char stop_marker[512];
    get_runtime_path(stop_marker, sizeof(stop_marker), "managed-stop");
    unlink(stop_marker);
    
    char self_path[512];
    ssize_t len = readlink("/proc/self/exe", self_path, sizeof(self_path) - 1);
    if (len != -1) {
        self_path[len] = '\0';
    } else {
        strcpy(self_path, "widgets_core");
    }
    
    char daemon_pid_f[512];
    get_runtime_path(daemon_pid_f, sizeof(daemon_pid_f), "daemon.pid");
    FILE *f_d = fopen(daemon_pid_f, "r");
    if (f_d) {
        int d_pid = 0;
        if (fscanf(f_d, "%d", &d_pid) == 1) {
            kill(d_pid, SIGTERM);
        }
        fclose(f_d);
        unlink(daemon_pid_f);
    }
    
    char daemon_cmd[1024];
    snprintf(daemon_cmd, sizeof(daemon_cmd), "nohup %s daemon >/tmp/widgets_daemon.log 2>&1 &", self_path);
    system(daemon_cmd);
    
    usleep(300000);
    system("hyprctl reload >/dev/null 2>&1");
    apply_widget_layouts();
}

void stop_widgets() {
    char stop_marker[512];
    get_runtime_path(stop_marker, sizeof(stop_marker), "managed-stop");
    FILE *f_sm = fopen(stop_marker, "w");
    if (f_sm) {
        fprintf(f_sm, "1\n");
        fclose(f_sm);
    }
    
    char daemon_pid_f[512];
    get_runtime_path(daemon_pid_f, sizeof(daemon_pid_f), "daemon.pid");
    FILE *f_d = fopen(daemon_pid_f, "r");
    if (f_d) {
        int d_pid = 0;
        if (fscanf(f_d, "%d", &d_pid) == 1) {
            kill(d_pid, SIGTERM);
        }
        fclose(f_d);
        unlink(daemon_pid_f);
    }
    
    save_widget_layouts();
    
    stop_widget("clock", "anto426.widget.clock");
    stop_widget("cava", "anto426.widget.cava");
    stop_widget("system", "anto426.widget.system");
    
    char customs_str[2048] = "";
    get_custom_widget_ids_str(customs_str, sizeof(customs_str));
    char *customs_copy = strdup(customs_str);
    char *saveptr_customs = NULL;
    char *tok = strtok_r(customs_copy, " \t\r\n", &saveptr_customs);
    while (tok) {
        stop_custom_widget(tok);
        tok = strtok_r(NULL, " \t\r\n", &saveptr_customs);
    }
    free(customs_copy);
    
    unlink(stop_marker);
}

void start_daemon_process() {
    stop_daemon_process();
    char self_path[512];
    ssize_t len = readlink("/proc/self/exe", self_path, sizeof(self_path) - 1);
    if (len != -1) self_path[len] = '\0';
    else strcpy(self_path, "widgets_core");
    
    char daemon_cmd[1024];
    snprintf(daemon_cmd, sizeof(daemon_cmd), "nohup %s daemon >/tmp/widgets_daemon.log 2>&1 &", self_path);
    system(daemon_cmd);
}

void stop_daemon_process() {
    char daemon_pid_f[512];
    get_runtime_path(daemon_pid_f, sizeof(daemon_pid_f), "daemon.pid");
    FILE *f_d = fopen(daemon_pid_f, "r");
    if (f_d) {
        int d_pid = 0;
        if (fscanf(f_d, "%d", &d_pid) == 1) {
            kill(d_pid, SIGTERM);
        }
        fclose(f_d);
        unlink(daemon_pid_f);
    }
}

int is_any_running() {
    char known_str[4096] = "";
    get_known_widget_ids_str(known_str, sizeof(known_str));

    char *known_copy = strdup(known_str);
    char *saveptr_known = NULL;
    char *tok = strtok_r(known_copy, " \t\r\n", &saveptr_known);
    while (tok) {
        if (is_widget_running(tok)) {
            free(known_copy);
            return 1;
        }
        tok = strtok_r(NULL, " \t\r\n", &saveptr_known);
    }
    free(known_copy);
    
    return 0;
}

void reap_children(void) {
    while (waitpid(-1, NULL, WNOHANG) > 0) {
    }
}

void run_daemon() {
    if (daemon(1, 1) != 0) {
        // Ignore error
    }
    char pid_f[512];
    get_runtime_path(pid_f, sizeof(pid_f), "daemon.pid");
    FILE *fp = fopen(pid_f, "w");
    if (fp) {
        fprintf(fp, "%d\n", getpid());
        fclose(fp);
    }
    
    char stop_marker[512];
    get_runtime_path(stop_marker, sizeof(stop_marker), "managed-stop");
    
    char config_f[512];
    get_env_path(config_f, sizeof(config_f), "anto426/widgets.env");
    
    char layout_f[512];
    get_env_path(layout_f, sizeof(layout_f), "anto426/widgets_layout.env");

    char last_unlocked_layout_sig[8192] = "";
    int have_unlocked_layout_sig = 0;
    
    while (1) {
        reap_children();

        struct stat st;
        if (stat(stop_marker, &st) == 0) {
            usleep(1000000);
            continue;
        }
        
        char widgets_str[4096] = "";
        get_known_widget_ids_str(widgets_str, sizeof(widgets_str));
        
        int locked = 0;
        char locked_val[16] = "0";
        if (read_env_value(config_f, "ANTO426_WIDGETS_LOCKED", locked_val, sizeof(locked_val))) {
            locked = (strcmp(locked_val, "1") == 0);
        }
        
        char widget_addresses[64][64];
        int widget_pids[64];
        char widget_ids[64][64];
        int widget_x[64];
        int widget_y[64];
        int widget_w[64];
        int widget_h[64];
        int widget_match_count[64];
        char widget_monitor[64][64];
        int widget_monitor_id[64];
        int widget_workspace[64];
        int total_widgets = 0;
        
        char *widgets_copy = strdup(widgets_str);
        char *saveptr_widgets = NULL;
        char *tok = strtok_r(widgets_copy, " \t\r\n", &saveptr_widgets);
        while (tok && total_widgets < 64) {
            snprintf(widget_ids[total_widgets], sizeof(widget_ids[total_widgets]), "%s", tok);
            widget_addresses[total_widgets][0] = '\0';
            widget_pids[total_widgets] = 0;
            widget_x[total_widgets] = 0;
            widget_y[total_widgets] = 0;
            widget_w[total_widgets] = 0;
            widget_h[total_widgets] = 0;
            widget_match_count[total_widgets] = 0;
            widget_monitor[total_widgets][0] = '\0';
            widget_monitor_id[total_widgets] = -1;
            widget_workspace[total_widgets] = 0;
            total_widgets++;
            tok = strtok_r(NULL, " \t\r\n", &saveptr_widgets);
        }
        free(widgets_copy);
        
        FILE *p_clients = popen("hyprctl clients -j | jq -r '.[] | \"\\(.class) \\(.initialClass) \\(.address) \\(.pid) \\(.at[0]) \\(.at[1]) \\(.size[0]) \\(.size[1]) \\(.monitor) \\(.workspace.id)\"'", "r");
        if (p_clients) {
            char line[1024];
            while (fgets(line, sizeof(line), p_clients)) {
                trim(line);
                char cl_class[256] = "", cl_init[256] = "", cl_addr[64] = "";
                int cl_pid = 0;
                int cl_x = 0, cl_y = 0, cl_w = 0, cl_h = 0;
                int cl_monitor_id = -1;
                int cl_ws = 0;
                char cl_monitor[64] = "";
                if (sscanf(line, "%255s %255s %63s %d %d %d %d %d %63s %d", cl_class, cl_init, cl_addr, &cl_pid, &cl_x, &cl_y, &cl_w, &cl_h, cl_monitor, &cl_ws) >= 9) {
                    if (isdigit((unsigned char)cl_monitor[0])) {
                        cl_monitor_id = atoi(cl_monitor);
                    }
                    for (int i = 0; i < total_widgets; i++) {
                        char target_class[256];
                        get_widget_class(widget_ids[i], target_class, sizeof(target_class));
                        if (strcmp(cl_class, target_class) == 0 || strcmp(cl_init, target_class) == 0) {
                            widget_match_count[i]++;
                            if (widget_addresses[i][0] == '\0') {
                                strcpy(widget_addresses[i], cl_addr);
                                widget_pids[i] = cl_pid;
                                widget_x[i] = cl_x;
                                widget_y[i] = cl_y;
                                widget_w[i] = cl_w;
                                widget_h[i] = cl_h;
                                snprintf(widget_monitor[i], sizeof(widget_monitor[i]), "%s", cl_monitor);
                                widget_monitor_id[i] = cl_monitor_id;
                                widget_workspace[i] = cl_ws;
                            }
                            break;
                        }
                    }
                }
            }
            pclose(p_clients);
        }

        for (int i = 0; i < total_widgets; i++) {
            if (widget_match_count[i] > 1) {
                dedupe_widget_windows(widget_ids[i]);
            }
        }

        if (!locked) {
            char layout_sig[8192] = "";
            for (int i = 0; i < total_widgets; i++) {
                if (widget_addresses[i][0] == '\0') continue;
                char part[256];
                snprintf(part, sizeof(part), "%s:%d,%d,%d,%d,%s;",
                         widget_ids[i], widget_x[i], widget_y[i], widget_w[i], widget_h[i], widget_monitor[i]);
                strncat(layout_sig, part, sizeof(layout_sig) - strlen(layout_sig) - 1);
            }

            if (layout_sig[0] != '\0') {
                if (!have_unlocked_layout_sig) {
                    snprintf(last_unlocked_layout_sig, sizeof(last_unlocked_layout_sig), "%s", layout_sig);
                    have_unlocked_layout_sig = 1;
                } else if (strcmp(layout_sig, last_unlocked_layout_sig) != 0) {
                    save_widget_layouts();
                    snprintf(last_unlocked_layout_sig, sizeof(last_unlocked_layout_sig), "%s", layout_sig);
                }
            }
        } else {
            have_unlocked_layout_sig = 0;
            last_unlocked_layout_sig[0] = '\0';
        }
        
        int occupied_workspaces[100];
        int occupied_count = 0;
        
        if (locked) {
            char jq_cmd[8192];
            snprintf(jq_cmd, sizeof(jq_cmd), "hyprctl clients -j | jq -r '.[] | select((.mapped // true) == true and (.hidden // false) == false and (.title // \"\") != \"\") | select(.class | test(\"^(anto426\\\\.widget\\\\.|clock-widget$|cava-widget$|system-widget$|rofi$|waybar$|swaync|swaync-control-center|wofi$|anto426-osd$)\"; \"i\") | not)");
            for (int i = 0; i < total_widgets; i++) {
                if (widget_pids[i] > 0) {
                    char add_pid[64];
                    snprintf(add_pid, sizeof(add_pid), " | select(.pid != %d)", widget_pids[i]);
                    strcat(jq_cmd, add_pid);
                }
            }
            strcat(jq_cmd, " | .workspace.id'");

            FILE *p_occ = popen(jq_cmd, "r");
            if (p_occ) {
                char buf[64];
                while (fgets(buf, sizeof(buf), p_occ) && occupied_count < 100) {
                    trim(buf);
                    if (buf[0] != '\0') {
                        occupied_workspaces[occupied_count++] = atoi(buf);
                    }
                }
                pclose(p_occ);
            }
        }
        
        char mon_names[16][64];
        int mon_ids[16];
        int mon_workspaces[16];
        int mon_focused[16];
        int total_monitors = 0;
        char focused_monitor[64] = "eDP-1";
        int focused_monitor_id = -1;
        
        if (locked) {
            FILE *p_mon = popen("hyprctl monitors -j | jq -r '.[] | \"\\(.id) \\(.name) \\(.activeWorkspace.id) \\(.focused)\"'", "r");
            if (p_mon) {
                char line[512];
                while (fgets(line, sizeof(line), p_mon) && total_monitors < 16) {
                    trim(line);
                    char m_name[64] = "";
                    int m_id = -1;
                    int m_ws = 1;
                    char m_foc[16] = "false";
                    if (sscanf(line, "%d %63s %d %15s", &m_id, m_name, &m_ws, m_foc) >= 3) {
                        mon_ids[total_monitors] = m_id;
                        strcpy(mon_names[total_monitors], m_name);
                        mon_workspaces[total_monitors] = m_ws;
                        mon_focused[total_monitors] = (strcmp(m_foc, "true") == 0);
                        if (mon_focused[total_monitors]) {
                            strcpy(focused_monitor, m_name);
                            focused_monitor_id = m_id;
                        }
                        total_monitors++;
                    }
                }
                pclose(p_mon);
            }
        }
        
        int music_active = is_music_active();
        
        for (int i = 0; i < total_widgets; i++) {
            char assigned_monitor[64] = "";
            int assigned_monitor_id = -1;
            int active_ws = 1;

            if (locked) {
                char monitor_key[256];
                make_layout_key(widget_ids[i], "MONITOR", monitor_key, sizeof(monitor_key));

                if (!read_env_value(layout_f, monitor_key, assigned_monitor, sizeof(assigned_monitor))) {
                    get_custom_widget_meta(widget_ids[i], "MONITOR", assigned_monitor, sizeof(assigned_monitor));
                }

                if (assigned_monitor[0] == '\0') {
                    strcpy(assigned_monitor, focused_monitor);
                    assigned_monitor_id = focused_monitor_id;
                }

                for (int m = 0; m < total_monitors; m++) {
                    if (strcmp(mon_names[m], assigned_monitor) == 0) {
                        assigned_monitor_id = mon_ids[m];
                        active_ws = mon_workspaces[m];
                        break;
                    }
                }
            }
            
            int is_occupied = 0;
            for (int o = 0; o < occupied_count; o++) {
                if (occupied_workspaces[o] == active_ws) {
                    is_occupied = 1;
                    break;
                }
            }
            
            int is_run = (widget_addresses[i][0] != '\0');
            
            if (locked && is_occupied) {
                if (is_run) {
                    char wm_class[256];
                    get_widget_class(widget_ids[i], wm_class, sizeof(wm_class));
                    stop_custom_widget(widget_ids[i]);
                    stop_widget(widget_ids[i], wm_class);
                }
            } else {
                int should_run = 1;
                if (strcmp(widget_ids[i], "cava") == 0 || strcmp(widget_ids[i], "spettro_audio") == 0) {
                    should_run = music_active;
                }
                
                if (should_run) {
                    if (!is_run) {
                        if (strcmp(widget_ids[i], "clock") == 0) {
                            launch_terminal("clock", "anto426.widget.clock", "Clock Widget", get_clock_command());
                        } else if (strcmp(widget_ids[i], "cava") == 0) {
                            launch_terminal("cava", "anto426.widget.cava", "Cava Widget", get_cava_command());
                        } else if (strcmp(widget_ids[i], "system") == 0) {
                            launch_terminal("system", "anto426.widget.system", "System Widget", get_system_command());
                        } else {
                            launch_custom_widget(widget_ids[i], locked);
                        }
                        usleep(150000);
                        reap_children();
                        if (locked) {
                            apply_widget_layout_single(widget_ids[i], assigned_monitor, active_ws, locked, NULL);
                        }
                    } else {
                        if (locked) {
                            char desired_w[16], desired_h[16], desired_x[16], desired_y[16];
                            char layout_val[16];
                            int needs_apply = 0;

                            get_widget_meta_defaults(widget_ids[i], desired_w, sizeof(desired_w), desired_h, sizeof(desired_h), desired_x, sizeof(desired_x), desired_y, sizeof(desired_y));
                            if (read_layout_value(widget_ids[i], "X", layout_val, sizeof(layout_val))) snprintf(desired_x, sizeof(desired_x), "%s", layout_val);
                            if (read_layout_value(widget_ids[i], "Y", layout_val, sizeof(layout_val))) snprintf(desired_y, sizeof(desired_y), "%s", layout_val);
                            if (read_layout_value(widget_ids[i], "W", layout_val, sizeof(layout_val))) snprintf(desired_w, sizeof(desired_w), "%s", layout_val);
                            if (read_layout_value(widget_ids[i], "H", layout_val, sizeof(layout_val))) snprintf(desired_h, sizeof(desired_h), "%s", layout_val);

                            if (abs(widget_x[i] - atoi(desired_x)) > 1 ||
                                abs(widget_y[i] - atoi(desired_y)) > 1 ||
                                abs(widget_w[i] - atoi(desired_w)) > 1 ||
                                abs(widget_h[i] - atoi(desired_h)) > 1 ||
                                widget_workspace[i] != active_ws ||
                                (assigned_monitor_id >= 0 && widget_monitor_id[i] >= 0 && widget_monitor_id[i] != assigned_monitor_id)) {
                                needs_apply = 1;
                            }

                            if (needs_apply) {
                                apply_widget_layout_single(widget_ids[i], assigned_monitor, active_ws, locked, widget_addresses[i]);
                            }
                        }
                    }
                } else {
                    if (is_run) {
                        char wm_class[256];
                        get_widget_class(widget_ids[i], wm_class, sizeof(wm_class));
                        stop_custom_widget(widget_ids[i]);
                        stop_widget(widget_ids[i], wm_class);
                    }
                }
            }
        }
        
        usleep(locked ? 1000000 : 3000000);
    }
}

int main(int argc, char **argv) {
    init_global_paths();
    
    if (argc < 2) {
        printf("Usage: widgets_core [start|stop|toggle|save-layout|apply-layout|write-rules|lock|unlock|toggle-lock|daemon|list-widgets|is-running|metadata|add-custom|remove-custom|status]\n");
        return 1;
    }
    
    char *cmd = argv[1];
    
    if (strcmp(cmd, "start") == 0) {
        start_widgets();
    } else if (strcmp(cmd, "stop") == 0) {
        stop_widgets();
    } else if (strcmp(cmd, "toggle") == 0) {
        int is_running = 0;
        char known_str[4096] = "";
        get_known_widget_ids_str(known_str, sizeof(known_str));

        char *known_copy = strdup(known_str);
        char *saveptr_known = NULL;
        char *tok = strtok_r(known_copy, " \t\r\n", &saveptr_known);
        while (tok) {
            char addr[64];
            if (get_widget_address(tok, addr, sizeof(addr))) {
                is_running = 1;
                break;
            }
            tok = strtok_r(NULL, " \t\r\n", &saveptr_known);
        }
        free(known_copy);
        
        char self_path[512];
        ssize_t len = readlink("/proc/self/exe", self_path, sizeof(self_path) - 1);
        if (len != -1) self_path[len] = '\0';
        else strcpy(self_path, "widgets_core");
        
        char run_cmd[1024];
        if (is_running) {
            snprintf(run_cmd, sizeof(run_cmd), "%s stop", self_path);
        } else {
            snprintf(run_cmd, sizeof(run_cmd), "%s start", self_path);
        }
        system(run_cmd);
    } else if (strcmp(cmd, "save-layout") == 0) {
        if (save_widget_layouts()) {
            apply_widget_layouts();
            printf("Layout saved\n");
        } else {
            printf("Failed to save layout\n");
        }
    } else if (strcmp(cmd, "apply-layout") == 0) {
        apply_widget_layouts();
    } else if (strcmp(cmd, "write-rules") == 0) {
        char config_f[512];
        char locked_val[16] = "0";
        get_env_path(config_f, sizeof(config_f), "anto426/widgets.env");
        read_env_value(config_f, "ANTO426_WIDGETS_LOCKED", locked_val, sizeof(locked_val));
        write_hypr_rules(strcmp(locked_val, "1") == 0);
    } else if (strcmp(cmd, "lock") == 0) {
        write_hypr_rules(1);
        char config_f[512];
        get_env_path(config_f, sizeof(config_f), "anto426/widgets.env");
        
        char temp_f[520];
        snprintf(temp_f, sizeof(temp_f), "%s.tmp", config_f);
        FILE *fin = fopen(config_f, "r");
        FILE *fout = fopen(temp_f, "w");
        if (fin && fout) {
            char line[MAX_LINE];
            while (fgets(line, sizeof(line), fin)) {
                if (strncmp(line, "export ANTO426_WIDGETS_LOCKED=", 30) == 0) {
                    fprintf(fout, "export ANTO426_WIDGETS_LOCKED=1\n");
                } else {
                    fputs(line, fout);
                }
            }
            fclose(fin);
            fclose(fout);
            rename(temp_f, config_f);
        }
        system("hyprctl reload >/dev/null 2>&1");
        apply_widget_layouts();
    } else if (strcmp(cmd, "unlock") == 0) {
        write_hypr_rules(0);
        char config_f[512];
        get_env_path(config_f, sizeof(config_f), "anto426/widgets.env");
        
        char temp_f[520];
        snprintf(temp_f, sizeof(temp_f), "%s.tmp", config_f);
        FILE *fin = fopen(config_f, "r");
        FILE *fout = fopen(temp_f, "w");
        if (fin && fout) {
            char line[MAX_LINE];
            while (fgets(line, sizeof(line), fin)) {
                if (strncmp(line, "export ANTO426_WIDGETS_LOCKED=", 30) == 0) {
                    fprintf(fout, "export ANTO426_WIDGETS_LOCKED=0\n");
                } else {
                    fputs(line, fout);
                }
            }
            fclose(fin);
            fclose(fout);
            rename(temp_f, config_f);
        }
        system("hyprctl reload >/dev/null 2>&1");
        show_all_widgets_unlocked();
    } else if (strcmp(cmd, "toggle-lock") == 0) {
        char config_f[512];
        get_env_path(config_f, sizeof(config_f), "anto426/widgets.env");
        char locked_val[16] = "0";
        read_env_value(config_f, "ANTO426_WIDGETS_LOCKED", locked_val, sizeof(locked_val));
        
        char self_path[512];
        ssize_t len = readlink("/proc/self/exe", self_path, sizeof(self_path) - 1);
        if (len != -1) self_path[len] = '\0';
        else strcpy(self_path, "widgets_core");
        
        char run_cmd[1024];
        if (strcmp(locked_val, "1") == 0) {
            snprintf(run_cmd, sizeof(run_cmd), "%s unlock", self_path);
        } else {
            snprintf(run_cmd, sizeof(run_cmd), "%s lock", self_path);
        }
        system(run_cmd);
    } else if (strcmp(cmd, "daemon") == 0) {
        run_daemon();
    } else if (strcmp(cmd, "list-widgets") == 0) {
        char order_str[4096] = "";
        get_widget_order_str(order_str, sizeof(order_str));

        char *order_copy = strdup(order_str);
        char *saveptr_order = NULL;
        char *tok = strtok_r(order_copy, " \t\r\n", &saveptr_order);
        while (tok) {
            printf("%s|%s\n", tok, get_widget_label(tok));
            tok = strtok_r(NULL, " \t\r\n", &saveptr_order);
        }
        free(order_copy);
    } else if (strcmp(cmd, "is-running") == 0) {
        if (argc > 2) {
            char addr[64];
            if (get_widget_address(argv[2], addr, sizeof(addr))) {
                printf("running\n");
                return 0;
            } else {
                printf("stopped\n");
                return 1;
            }
        }
    } else if (strcmp(cmd, "metadata") == 0) {
        if (argc > 3) {
            char val[MAX_VAL] = "";
            char meta_f[512];
            get_env_path(meta_f, sizeof(meta_f), "");
            char fpath[1024];
            snprintf(fpath, sizeof(fpath), "%s/anto426/widgets.d/%s.env", meta_f, argv[2]);
            if (read_env_value(fpath, argv[3], val, sizeof(val))) {
                printf("%s\n", val);
            } else {
                if (strcmp(argv[2], "clock") == 0) {
                    if (strcmp(argv[3], "ANTO426_WIDGET_CLASS") == 0) printf("anto426.widget.clock\n");
                    else if (strcmp(argv[3], "ANTO426_WIDGET_NAME") == 0) printf("Clock Widget\n");
                    else if (strcmp(argv[3], "ANTO426_WIDGET_MODE") == 0) printf("terminal\n");
                    else if (strcmp(argv[3], "ANTO426_WIDGET_WIDTH") == 0) printf("300\n");
                    else if (strcmp(argv[3], "ANTO426_WIDGET_HEIGHT") == 0) printf("140\n");
                } else if (strcmp(argv[2], "cava") == 0) {
                    if (strcmp(argv[3], "ANTO426_WIDGET_CLASS") == 0) printf("anto426.widget.cava\n");
                    else if (strcmp(argv[3], "ANTO426_WIDGET_NAME") == 0) printf("Cava Audio Visualizer\n");
                    else if (strcmp(argv[3], "ANTO426_WIDGET_MODE") == 0) printf("terminal\n");
                    else if (strcmp(argv[3], "ANTO426_WIDGET_WIDTH") == 0) printf("500\n");
                    else if (strcmp(argv[3], "ANTO426_WIDGET_HEIGHT") == 0) printf("168\n");
                } else if (strcmp(argv[2], "system") == 0) {
                    if (strcmp(argv[3], "ANTO426_WIDGET_CLASS") == 0) printf("anto426.widget.system\n");
                    else if (strcmp(argv[3], "ANTO426_WIDGET_NAME") == 0) printf("System Monitor\n");
                    else if (strcmp(argv[3], "ANTO426_WIDGET_MODE") == 0) printf("terminal\n");
                    else if (strcmp(argv[3], "ANTO426_WIDGET_WIDTH") == 0) printf("380\n");
                    else if (strcmp(argv[3], "ANTO426_WIDGET_HEIGHT") == 0) printf("210\n");
                }
            }
        }
    } else if (strcmp(cmd, "add-custom") == 0) {
        if (argc >= 6) {
            const char *w_id = argv[2];
            const char *name = argv[3];
            const char *wm_class = argv[4];
            const char *command = argv[5];
            const char *mode = (argc > 6) ? argv[6] : "app";
            const char *w = (argc > 7) ? argv[7] : "520";
            const char *h = (argc > 8) ? argv[8] : "360";
            const char *monitor = (argc > 9) ? argv[9] : "";
            
            write_custom_widget_env(w_id, name, wm_class, command, mode, w, h, monitor);
            
            char customs_str[2048] = "";
            get_custom_widget_ids_str(customs_str, sizeof(customs_str));
            
            if (!token_list_contains(customs_str, w_id)) {
                char new_customs[2048] = "";
                snprintf(new_customs, sizeof(new_customs), "%s", customs_str);
                append_unique_token(new_customs, sizeof(new_customs), w_id);
                save_custom_widget_ids_str(new_customs);
            }
            
            char config_f[512];
            get_env_path(config_f, sizeof(config_f), "anto426/widgets.env");
            char locked_val[16] = "0";
            read_env_value(config_f, "ANTO426_WIDGETS_LOCKED", locked_val, sizeof(locked_val));
            write_hypr_rules(strcmp(locked_val, "1") == 0);
        }
    } else if (strcmp(cmd, "remove-custom") == 0) {
        if (argc > 2) {
            const char *w_id = argv[2];
            char customs_str[2048] = "";
            get_custom_widget_ids_str(customs_str, sizeof(customs_str));

            stop_custom_widget(w_id);
            
            char new_customs[2048] = "";
            char *copy = strdup(customs_str);
            char *saveptr_customs = NULL;
            char *tok = strtok_r(copy, " \t\r\n", &saveptr_customs);
            while (tok) {
                if (strcmp(tok, w_id) != 0) {
                    if (new_customs[0] != '\0') {
                        strcat(new_customs, " ");
                    }
                    strcat(new_customs, tok);
                }
                tok = strtok_r(NULL, " \t\r\n", &saveptr_customs);
            }
            free(copy);
            save_custom_widget_ids_str(new_customs);
            
            char meta_fpath[1024];
            char env_dir[512];
            get_env_path(env_dir, sizeof(env_dir), "");
            snprintf(meta_fpath, sizeof(meta_fpath), "%s/anto426/widgets.d/%.200s.env", env_dir, w_id);
            unlink(meta_fpath);
            remove_widget_layout_vars(w_id);
            
            char config_f[512];
            get_env_path(config_f, sizeof(config_f), "anto426/widgets.env");
            char locked_val[16] = "0";
            read_env_value(config_f, "ANTO426_WIDGETS_LOCKED", locked_val, sizeof(locked_val));
            write_hypr_rules(strcmp(locked_val, "1") == 0);
        }
    } else if (strcmp(cmd, "status") == 0) {
        int is_run = 0;
        char known_str[4096] = "";
        get_known_widget_ids_str(known_str, sizeof(known_str));

        char *known_copy = strdup(known_str);
        char *saveptr_known = NULL;
        char *tok = strtok_r(known_copy, " \t\r\n", &saveptr_known);
        while (tok) {
            char addr[64];
            if (get_widget_address(tok, addr, sizeof(addr))) {
                is_run = 1;
                break;
            }
            tok = strtok_r(NULL, " \t\r\n", &saveptr_known);
        }
        free(known_copy);
        
        if (is_run) printf("running\n");
        else printf("stopped\n");
    }
    
    return 0;
}
