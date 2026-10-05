#include "design_tokens.h"
#define _GNU_SOURCE

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <glib.h>
#include <sys/file.h>
#include <limits.h>
#include <math.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#if defined(__GNUC__)
#pragma GCC diagnostic ignored "-Wformat-truncation"
#endif

typedef struct {
    int r;
    int g;
    int b;
    char hex[8];
} Color;

typedef struct {
    Color background;
    Color surface;
    Color select;
    Color accent;
    Color border;
    Color foreground;
    Color muted;
    Color red;
    Color orange;
    Color yellow;
    Color green;
    Color pink;
    Color purple;
    Color gray;
    Color base;
    Color base_alt;
    Color titlebar;
    Color titlebar_backdrop;
    Color popover;
    Color selected_fg;

    char panel_bg[80];
    char panel_bg_hover[80];
    char overlay_bg[80];
    char item_bg[80];
    char item_bg_hover[80];
    char item_bg_active[80];
    char accent_soft[80];
    char accent_strong[80];
    char border_soft[80];
    char border_medium[80];
    char shadow_soft[80];
    char shadow_medium[80];
    char background_alpha[80];
    char surface_alpha[80];
    char select_alpha[80];
    char accent_alpha[80];
    char border_alpha[80];
} Palette;

typedef struct {
    char home[PATH_MAX];
    char script_dir[PATH_MAX];
    char state_dir[PATH_MAX];
    char cache_awww[PATH_MAX];
    char selector_cache_dir[PATH_MAX];
    char local_config_root[PATH_MAX];
    char wallpaper_config_dir[PATH_MAX];
    char wallpaper_output_dir[PATH_MAX];
    char wallpaper_current_state[PATH_MAX];
    char desktop_palette_state[PATH_MAX];
    char boot_login_state[PATH_MAX];
    char boot_login_stage_dir[PATH_MAX];
    char colors_dir[PATH_MAX];
    char hypr_theme_file[PATH_MAX];
    char ghostty_theme_dir[PATH_MAX];
    char ghostty_dynamic_file[PATH_MAX];
    char htop_config_dir[PATH_MAX];
    char btop_config_dir[PATH_MAX];
    char btop_theme_dir[PATH_MAX];
    char gtk3_dir[PATH_MAX];
    char gtk4_dir[PATH_MAX];
    char kvantum_dir[PATH_MAX];
    char kvantum_theme_dir[PATH_MAX];
    char kvantum_config_file[PATH_MAX];
    char qt5ct_dir[PATH_MAX];
    char qt6ct_dir[PATH_MAX];
    char effects_script[PATH_MAX];
    char modules_script[PATH_MAX];
    char daemon_script[PATH_MAX];
    char widgets_script[PATH_MAX];
    char grub_theme_dir[PATH_MAX];
    char grub_background[PATH_MAX];
    char grub_theme[PATH_MAX];
    char grub_select_c[PATH_MAX];
    char grub_select_e[PATH_MAX];
    char grub_select_w[PATH_MAX];
    char sddm_background[PATH_MAX];
    char sddm_theme[PATH_MAX];
    char log_file[PATH_MAX];
} Paths;

typedef struct {
    int width;
    int height;
    char text[64];
} Canvas;

#define MAX_WALLPAPER_OUTPUTS 64
#define MAX_WALLPAPER_ASSETS 64

typedef struct {
    char output[256];
    char kind[16];
    char asset[PATH_MAX];
    char sample_path[PATH_MAX];
    double logical_area;
    int avg_r;
    int avg_g;
    int avg_b;
    int accent_r;
    int accent_g;
    int accent_b;
    int sampled;
} WallpaperAssignment;

typedef struct {
    char output[256];
    int width;
    int height;
    double scale;
} OutputGeometry;

typedef struct {
    char asset[PATH_MAX];
    char kind[16];
    char sample_path[PATH_MAX];
    double weight;
    int avg_r;
    int avg_g;
    int avg_b;
    int accent_r;
    int accent_g;
    int accent_b;
} WallpaperAssetSample;

typedef struct {
    WallpaperAssignment assignments[MAX_WALLPAPER_OUTPUTS];
    size_t assignment_count;
    WallpaperAssetSample assets[MAX_WALLPAPER_ASSETS];
    size_t asset_count;
    int average_r;
    int average_g;
    int average_b;
    int accent_r;
    int accent_g;
    int accent_b;
    size_t dominant_asset;
} WallpaperPaletteAggregate;

static const char *vscode_theme_name = "Anto426 Rofi Dynamic";
static const char *vscode_theme_file = "Anto426-Rofi-Dynamic.json";
static const char *live_options_version = "5";

static unsigned long long fnv1a_hash(const char *s);
static int effects_job_is_stale(const Paths *paths, const char *expected);
static int extract_average_rgb(const char *src, int *r, int *g, int *b);
static int extract_vibrant_rgb(const char *src, int *out_r, int *out_g,
                               int *out_b);
static int compare_assignment_output(const void *left, const void *right);
static int aggregate_wallpaper_samples(WallpaperPaletteAggregate *aggregate);

static void copy_string(char *dst, size_t dst_size, const char *src) {
    if (dst_size == 0) {
        return;
    }
    if (!src) {
        dst[0] = '\0';
        return;
    }
    snprintf(dst, dst_size, "%s", src);
}

static void trim_newline(char *s) {
    size_t len;
    if (!s) {
        return;
    }
    len = strlen(s);
    while (len > 0 && (s[len - 1] == '\n' || s[len - 1] == '\r' || s[len - 1] == ' ' || s[len - 1] == '\t')) {
        s[len - 1] = '\0';
        len--;
    }
}

static long long monotonic_ms(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        return 0;
    }
    return ((long long)ts.tv_sec * 1000LL) + (ts.tv_nsec / 1000000LL);
}

static int starts_with(const char *s, const char *prefix) {
    return s && prefix && strncmp(s, prefix, strlen(prefix)) == 0;
}

static int regular_file_exists(const char *path) {
    struct stat st;
    return path && stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

static int dir_exists(const char *path) {
    struct stat st;
    return path && stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

static int nonempty_file_exists(const char *path) {
    struct stat st;
    return path && stat(path, &st) == 0 && S_ISREG(st.st_mode) && st.st_size > 0;
}

static int mkdir_p(const char *path) {
    char tmp[PATH_MAX];
    size_t len;

    if (!path || !path[0]) {
        return -1;
    }

    copy_string(tmp, sizeof(tmp), path);
    len = strlen(tmp);
    if (len == 0) {
        return -1;
    }
    if (tmp[len - 1] == '/') {
        tmp[len - 1] = '\0';
    }

    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
                return -1;
            }
            *p = '/';
        }
    }
    if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
        return -1;
    }
    return 0;
}

static void dirname_into(const char *path, char *dst, size_t dst_size) {
    const char *slash;
    size_t len;

    if (!path || !path[0]) {
        copy_string(dst, dst_size, ".");
        return;
    }
    slash = strrchr(path, '/');
    if (!slash) {
        copy_string(dst, dst_size, ".");
        return;
    }
    if (slash == path) {
        copy_string(dst, dst_size, "/");
        return;
    }
    len = (size_t)(slash - path);
    if (len >= dst_size) {
        len = dst_size - 1;
    }
    memcpy(dst, path, len);
    dst[len] = '\0';
}

static int ensure_parent_dir(const char *path) {
    char dir[PATH_MAX];
    dirname_into(path, dir, sizeof(dir));
    return mkdir_p(dir);
}

static char *shell_quote(const char *s) {
    size_t extra = 3;
    char *out;
    char *p;

    if (!s) {
        s = "";
    }
    for (const char *c = s; *c; c++) {
        extra += (*c == '\'') ? 4 : 1;
    }

    out = malloc(extra);
    if (!out) {
        return NULL;
    }
    p = out;
    *p++ = '\'';
    for (const char *c = s; *c; c++) {
        if (*c == '\'') {
            memcpy(p, "'\\''", 4);
            p += 4;
        } else {
            *p++ = *c;
        }
    }
    *p++ = '\'';
    *p = '\0';
    return out;
}

static int run_shell(const char *cmd) {
    int status;
    if (!cmd || !cmd[0]) {
        return 0;
    }
    status = system(cmd);
    return status != -1 && WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

static int capture_command(const char *cmd, char *out, size_t out_size) {
    FILE *fp;
    size_t used = 0;
    int status;

    if (out_size > 0) {
        out[0] = '\0';
    }
    fp = popen(cmd, "r");
    if (!fp) {
        return 0;
    }

    while (out_size > 1 && !feof(fp)) {
        size_t n = fread(out + used, 1, out_size - used - 1, fp);
        used += n;
        out[used] = '\0';
        if (used >= out_size - 1) {
            break;
        }
    }

    status = pclose(fp);
    trim_newline(out);
    return status != -1 && WIFEXITED(status) && WEXITSTATUS(status) == 0 && out[0] != '\0';
}

static int command_exists(const char *cmd) {
    char shell[512];
    snprintf(shell, sizeof(shell), "command -v %s >/dev/null 2>&1", cmd);
    return run_shell(shell);
}

static void append_log(const Paths *paths, const char *fmt, ...) {
    FILE *f;
    time_t now;
    struct tm tm_info;
    char time_buf[64];
    va_list ap;

    mkdir_p(paths->state_dir);
    f = fopen(paths->log_file, "a");
    if (!f) {
        return;
    }

    now = time(NULL);
    localtime_r(&now, &tm_info);
    strftime(time_buf, sizeof(time_buf), "%F %T", &tm_info);
    fprintf(f, "[%s] ", time_buf);
    va_start(ap, fmt);
    vfprintf(f, fmt, ap);
    va_end(ap);
    fputc('\n', f);
    fclose(f);
}

static void notify_send(const char *title, const char *body) {
    char *qtitle;
    char *qbody;
    char cmd[8192];

    if (!command_exists("notify-send")) {
        return;
    }
    qtitle = shell_quote(title);
    qbody = shell_quote(body);
    if (!qtitle || !qbody) {
        free(qtitle);
        free(qbody);
        return;
    }
    snprintf(cmd, sizeof(cmd), "notify-send %s %s >/dev/null 2>&1", qtitle, qbody);
    run_shell(cmd);
    free(qtitle);
    free(qbody);
}

static void init_paths(Paths *paths, const char *log_name) {
    const char *home = getenv("HOME");
    const char *state_home = getenv("XDG_STATE_HOME");
    const char *cache_home = getenv("XDG_CACHE_HOME");
    const char *config_home = getenv("XDG_CONFIG_HOME");
    const char *local_config_override = getenv("ANTO_LOCAL_CONFIG_ROOT");
    const char *script_dir_override = getenv("ANTO426_SCRIPT_DIR");
    const char *sddm_theme_dir_env = getenv("ANTO426_SDDM_THEME_DIR");
    char config_root[PATH_MAX];
    char local_config_root[PATH_MAX];
    char sddm_theme_dir[PATH_MAX];

    memset(paths, 0, sizeof(*paths));
    copy_string(paths->home, sizeof(paths->home), home && home[0] ? home : "/tmp");

    if (script_dir_override && script_dir_override[0]) {
        copy_string(paths->script_dir, sizeof(paths->script_dir), script_dir_override);
    } else if (config_home && config_home[0]) {
        snprintf(paths->script_dir, sizeof(paths->script_dir), "%s/anto426", config_home);
    } else {
        snprintf(paths->script_dir, sizeof(paths->script_dir), "%s/.config/anto426", paths->home);
    }

    if (state_home && state_home[0]) {
        snprintf(paths->state_dir, sizeof(paths->state_dir), "%s/anto426", state_home);
    } else {
        snprintf(paths->state_dir, sizeof(paths->state_dir), "%s/.local/state/anto426", paths->home);
    }

    if (cache_home && cache_home[0]) {
        snprintf(paths->cache_awww, sizeof(paths->cache_awww), "%s/awww", cache_home);
        snprintf(paths->selector_cache_dir,
                 sizeof(paths->selector_cache_dir),
                 "%s/anto426/wallpaper-selector", cache_home);
    } else {
        snprintf(paths->cache_awww, sizeof(paths->cache_awww), "%s/.cache/awww", paths->home);
        snprintf(paths->selector_cache_dir,
                 sizeof(paths->selector_cache_dir),
                 "%s/.cache/anto426/wallpaper-selector", paths->home);
    }

    if (config_home && config_home[0])
        copy_string(config_root, sizeof(config_root), config_home);
    else
        snprintf(config_root, sizeof(config_root), "%s/.config", paths->home);

    if (local_config_override && local_config_override[0])
        copy_string(local_config_root, sizeof(local_config_root),
                    local_config_override);
    else
        snprintf(local_config_root, sizeof(local_config_root),
                 "%s/anto426-local", config_root);

    copy_string(paths->local_config_root, sizeof(paths->local_config_root),
                local_config_root);
    snprintf(paths->wallpaper_config_dir,
             sizeof(paths->wallpaper_config_dir), "%s/wallpaper",
             local_config_root);
    snprintf(paths->wallpaper_output_dir,
             sizeof(paths->wallpaper_output_dir), "%s/outputs",
             paths->wallpaper_config_dir);
    snprintf(paths->wallpaper_current_state,
             sizeof(paths->wallpaper_current_state), "%s/current.state",
             paths->wallpaper_config_dir);
    snprintf(paths->desktop_palette_state,
             sizeof(paths->desktop_palette_state), "%s/desktop-palette.env",
             paths->wallpaper_config_dir);
    snprintf(paths->boot_login_state, sizeof(paths->boot_login_state),
             "%s/boot-login.env", paths->wallpaper_config_dir);
    snprintf(paths->boot_login_stage_dir,
             sizeof(paths->boot_login_stage_dir), "%s/boot-login",
             paths->wallpaper_config_dir);

    snprintf(paths->colors_dir, sizeof(paths->colors_dir), "%s/theme",
             local_config_root);
    snprintf(paths->hypr_theme_file, sizeof(paths->hypr_theme_file),
             "%s/hypr/theme.generated.conf", local_config_root);
    snprintf(paths->ghostty_dynamic_file,
             sizeof(paths->ghostty_dynamic_file),
             "%s/ghostty/dynamic.conf", local_config_root);

    if (config_home && config_home[0]) {
        snprintf(paths->ghostty_theme_dir, sizeof(paths->ghostty_theme_dir), "%s/ghostty/themes", config_home);
        snprintf(paths->htop_config_dir, sizeof(paths->htop_config_dir), "%s/htop", config_home);
        snprintf(paths->btop_config_dir, sizeof(paths->btop_config_dir), "%s/btop", config_home);
        snprintf(paths->gtk3_dir, sizeof(paths->gtk3_dir), "%s/gtk-3.0", config_home);
        snprintf(paths->gtk4_dir, sizeof(paths->gtk4_dir), "%s/gtk-4.0", config_home);
        snprintf(paths->kvantum_dir, sizeof(paths->kvantum_dir), "%s/Kvantum", config_home);
        snprintf(paths->qt5ct_dir, sizeof(paths->qt5ct_dir), "%s/qt5ct", config_home);
        snprintf(paths->qt6ct_dir, sizeof(paths->qt6ct_dir), "%s/qt6ct", config_home);
    } else {
        snprintf(paths->ghostty_theme_dir, sizeof(paths->ghostty_theme_dir), "%s/.config/ghostty/themes", paths->home);
        snprintf(paths->htop_config_dir, sizeof(paths->htop_config_dir), "%s/.config/htop", paths->home);
        snprintf(paths->btop_config_dir, sizeof(paths->btop_config_dir), "%s/.config/btop", paths->home);
        snprintf(paths->gtk3_dir, sizeof(paths->gtk3_dir), "%s/.config/gtk-3.0", paths->home);
        snprintf(paths->gtk4_dir, sizeof(paths->gtk4_dir), "%s/.config/gtk-4.0", paths->home);
        snprintf(paths->kvantum_dir, sizeof(paths->kvantum_dir), "%s/.config/Kvantum", paths->home);
        snprintf(paths->qt5ct_dir, sizeof(paths->qt5ct_dir), "%s/.config/qt5ct", paths->home);
        snprintf(paths->qt6ct_dir, sizeof(paths->qt6ct_dir), "%s/.config/qt6ct", paths->home);
    }

    snprintf(paths->btop_theme_dir, sizeof(paths->btop_theme_dir), "%s/themes", paths->btop_config_dir);
    snprintf(paths->kvantum_theme_dir, sizeof(paths->kvantum_theme_dir), "%s/anto426", paths->kvantum_dir);
    snprintf(paths->kvantum_config_file, sizeof(paths->kvantum_config_file), "%s/kvantum.kvconfig", paths->kvantum_dir);
    snprintf(paths->effects_script, sizeof(paths->effects_script), "%s/wallpaper_effects.sh", paths->script_dir);
    snprintf(paths->modules_script, sizeof(paths->modules_script), "%s/wallpaper_effects_modules.sh", paths->script_dir);
    snprintf(paths->daemon_script, sizeof(paths->daemon_script), "%s/wallpaper_daemon.sh", paths->script_dir);
    snprintf(paths->widgets_script, sizeof(paths->widgets_script), "%s/widgets.sh", paths->script_dir);
    snprintf(paths->grub_theme_dir, sizeof(paths->grub_theme_dir), "/usr/share/grub/themes/anto426");
    snprintf(paths->grub_background, sizeof(paths->grub_background), "%s/background.jpg", paths->grub_theme_dir);
    snprintf(paths->grub_theme, sizeof(paths->grub_theme), "%s/theme.txt", paths->grub_theme_dir);
    snprintf(paths->grub_select_c, sizeof(paths->grub_select_c), "%s/select_c.png", paths->grub_theme_dir);
    snprintf(paths->grub_select_e, sizeof(paths->grub_select_e), "%s/select_e.png", paths->grub_theme_dir);
    snprintf(paths->grub_select_w, sizeof(paths->grub_select_w), "%s/select_w.png", paths->grub_theme_dir);
    copy_string(sddm_theme_dir, sizeof(sddm_theme_dir),
                sddm_theme_dir_env && sddm_theme_dir_env[0] ? sddm_theme_dir_env : "/usr/share/sddm/themes/anto426-sddm");
    snprintf(paths->sddm_background, sizeof(paths->sddm_background), "%s/Backgrounds/anto426-current.png", sddm_theme_dir);
    snprintf(paths->sddm_theme, sizeof(paths->sddm_theme), "%s/theme.conf", sddm_theme_dir);
    snprintf(paths->log_file, sizeof(paths->log_file), "%s/%s", paths->state_dir, log_name);
}

static void ensure_runtime_dirs(const Paths *paths) {
    mkdir_p(paths->state_dir);
    mkdir_p(paths->cache_awww);
    mkdir_p(paths->wallpaper_config_dir);
    mkdir_p(paths->boot_login_stage_dir);
    mkdir_p(paths->colors_dir);
    ensure_parent_dir(paths->hypr_theme_file);
    mkdir_p(paths->ghostty_theme_dir);
    ensure_parent_dir(paths->ghostty_dynamic_file);
    mkdir_p(paths->htop_config_dir);
    mkdir_p(paths->btop_theme_dir);
    mkdir_p(paths->gtk3_dir);
    mkdir_p(paths->gtk4_dir);
    mkdir_p(paths->kvantum_theme_dir);
    mkdir_p(paths->qt5ct_dir);
    mkdir_p(paths->qt6ct_dir);
    mkdir_p(paths->qt5ct_dir);
    mkdir_p(paths->qt6ct_dir);
}

static int normalize_existing_path(const char *input, char *out, size_t out_size) {
    char resolved[PATH_MAX];
    if (!input || !input[0]) {
        if (out_size) {
            out[0] = '\0';
        }
        return 0;
    }
    if (realpath(input, resolved)) {
        copy_string(out, out_size, resolved);
        return 1;
    }
    copy_string(out, out_size, input);
    return 0;
}

static int write_text_file(const char *path, const char *fmt, ...) {
    char temporary[PATH_MAX + 32];
    struct stat target_stat;
    mode_t mode = 0644;
    int fd;
    FILE *f;
    va_list ap;
    int ok = 1;

    if (ensure_parent_dir(path) != 0) {
        return 0;
    }
    if (snprintf(temporary, sizeof(temporary), "%s.tmp.XXXXXX", path) >=
        (int)sizeof(temporary)) {
        return 0;
    }
    if (stat(path, &target_stat) == 0) {
        mode = target_stat.st_mode & 0777;
    }
    fd = mkstemp(temporary);
    if (fd < 0) {
        return 0;
    }
    (void)fchmod(fd, mode);
    f = fdopen(fd, "w");
    if (!f) {
        close(fd);
        unlink(temporary);
        return 0;
    }
    va_start(ap, fmt);
    if (vfprintf(f, fmt, ap) < 0) {
        ok = 0;
    }
    va_end(ap);
    if (fflush(f) != 0) ok = 0;
    if (fsync(fileno(f)) != 0) ok = 0;
    if (fclose(f) != 0) ok = 0;
    if (ok && rename(temporary, path) == 0) {
        return 1;
    }
    unlink(temporary);
    return 0;
}

static int read_first_line(const char *path, char *out, size_t out_size) {
    FILE *f;
    if (out_size) {
        out[0] = '\0';
    }
    f = fopen(path, "r");
    if (!f) {
        return 0;
    }
    if (!fgets(out, (int)out_size, f)) {
        fclose(f);
        return 0;
    }
    fclose(f);
    trim_newline(out);
    return out[0] != '\0';
}

static char *read_file_alloc(const char *path, size_t *size_out) {
    FILE *f;
    long size;
    char *data;
    size_t n;

    if (size_out) {
        *size_out = 0;
    }
    f = fopen(path, "rb");
    if (!f) {
        return NULL;
    }
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return NULL;
    }
    size = ftell(f);
    if (size < 0) {
        fclose(f);
        return NULL;
    }
    if (fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return NULL;
    }
    data = malloc((size_t)size + 1);
    if (!data) {
        fclose(f);
        return NULL;
    }
    n = fread(data, 1, (size_t)size, f);
    fclose(f);
    data[n] = '\0';
    if (size_out) {
        *size_out = n;
    }
    return data;
}

static int process_is_running(const char *name) {
    DIR *dir;
    struct dirent *entry;
    int found = 0;

    dir = opendir("/proc");
    if (!dir) {
        return 0;
    }
    while ((entry = readdir(dir)) != NULL) {
        char *endptr = NULL;
        char comm_path[PATH_MAX];
        char comm[256];
        FILE *f;
        (void)strtol(entry->d_name, &endptr, 10);
        if (!endptr || *endptr != '\0') {
            continue;
        }
        snprintf(comm_path, sizeof(comm_path), "/proc/%s/comm", entry->d_name);
        f = fopen(comm_path, "r");
        if (!f) {
            continue;
        }
        if (fgets(comm, sizeof(comm), f)) {
            trim_newline(comm);
            if (strcmp(comm, name) == 0) {
                found = 1;
                fclose(f);
                break;
            }
        }
        fclose(f);
    }
    closedir(dir);
    return found;
}

static int current_mpvpaper_path(char *out, size_t out_size) {
    DIR *dir;
    struct dirent *entry;

    if (out_size) {
        out[0] = '\0';
    }

    dir = opendir("/proc");
    if (!dir) {
        return 0;
    }

    while ((entry = readdir(dir)) != NULL) {
        char *endptr = NULL;
        char comm_path[PATH_MAX];
        char cmd_path[PATH_MAX];
        char comm[256];
        FILE *f;
        char data[16384];
        size_t n;
        char last[PATH_MAX] = "";

        (void)strtol(entry->d_name, &endptr, 10);
        if (!endptr || *endptr != '\0') {
            continue;
        }
        snprintf(comm_path, sizeof(comm_path), "/proc/%s/comm", entry->d_name);
        f = fopen(comm_path, "r");
        if (!f) {
            continue;
        }
        if (!fgets(comm, sizeof(comm), f)) {
            fclose(f);
            continue;
        }
        fclose(f);
        trim_newline(comm);
        if (strcmp(comm, "mpvpaper") != 0) {
            continue;
        }

        snprintf(cmd_path, sizeof(cmd_path), "/proc/%s/cmdline", entry->d_name);
        f = fopen(cmd_path, "rb");
        if (!f) {
            continue;
        }
        n = fread(data, 1, sizeof(data) - 1, f);
        fclose(f);
        data[n] = '\0';
        for (size_t i = 0; i < n;) {
            char *arg = &data[i];
            size_t len = strlen(arg);
            if (len > 0 && regular_file_exists(arg)) {
                copy_string(last, sizeof(last), arg);
            }
            i += len + 1;
        }
        if (last[0]) {
            normalize_existing_path(last, out, out_size);
            closedir(dir);
            return 1;
        }
    }
    closedir(dir);
    return 0;
}

static int output_name_is_safe(const char *output) {
    size_t length;

    if (!output || !output[0]) {
        return 0;
    }
    length = strlen(output);
    if (length >= 256) {
        return 0;
    }
    for (const unsigned char *cursor = (const unsigned char *)output; *cursor; cursor++) {
        if (*cursor < 0x20 || *cursor == 0x7f) {
            return 0;
        }
    }
    return 1;
}

static int mpvpaper_process_matches(const char *output, const char *wallpaper,
                                    int include_global_output) {
    DIR *dir;
    struct dirent *entry;
    int found = 0;

    dir = opendir("/proc");
    if (!dir) {
        return 0;
    }

    while ((entry = readdir(dir)) != NULL) {
        char *endptr = NULL;
        char comm_path[PATH_MAX];
        char cmd_path[PATH_MAX];
        char comm[256];
        char data[16384];
        FILE *f;
        size_t n;
        int output_matches = 0;
        int wallpaper_matches = wallpaper == NULL;

        (void)strtol(entry->d_name, &endptr, 10);
        if (!endptr || *endptr != '\0') {
            continue;
        }
        snprintf(comm_path, sizeof(comm_path), "/proc/%s/comm", entry->d_name);
        f = fopen(comm_path, "r");
        if (!f) {
            continue;
        }
        if (!fgets(comm, sizeof(comm), f)) {
            fclose(f);
            continue;
        }
        fclose(f);
        trim_newline(comm);
        if (strcmp(comm, "mpvpaper") != 0) {
            continue;
        }

        snprintf(cmd_path, sizeof(cmd_path), "/proc/%s/cmdline", entry->d_name);
        f = fopen(cmd_path, "rb");
        if (!f) {
            continue;
        }
        n = fread(data, 1, sizeof(data) - 1, f);
        fclose(f);
        data[n] = '\0';

        for (size_t i = 0; i < n;) {
            char normalized[PATH_MAX];
            char *arg = &data[i];
            size_t len = strlen(arg);

            if (strcmp(output, "ALL") == 0) {
                if (strcmp(arg, "ALL") == 0 || strcmp(arg, "*") == 0) {
                    output_matches = 1;
                }
            } else if (strcmp(arg, output) == 0 ||
                       (include_global_output &&
                        (strcmp(arg, "ALL") == 0 || strcmp(arg, "*") == 0))) {
                output_matches = 1;
            }

            if (wallpaper && regular_file_exists(arg)) {
                normalize_existing_path(arg, normalized, sizeof(normalized));
                if (strcmp(normalized, wallpaper) == 0) {
                    wallpaper_matches = 1;
                }
            }
            i += len + 1;
        }
        if (output_matches && wallpaper_matches) {
            found = 1;
            break;
        }
    }
    closedir(dir);
    return found;
}

static void stop_mpvpaper_for_output(const Paths *paths, const char *output) {
    DIR *dir;
    struct dirent *entry;
    int stopped = 0;

    dir = opendir("/proc");
    if (!dir) {
        return;
    }

    while ((entry = readdir(dir)) != NULL) {
        char *endptr = NULL;
        char comm_path[PATH_MAX];
        char cmd_path[PATH_MAX];
        char comm[256];
        char data[16384];
        FILE *f;
        size_t n;
        long pid;
        int matches = strcmp(output, "ALL") == 0;

        pid = strtol(entry->d_name, &endptr, 10);
        if (!endptr || *endptr != '\0' || pid <= 1) {
            continue;
        }
        snprintf(comm_path, sizeof(comm_path), "/proc/%s/comm", entry->d_name);
        f = fopen(comm_path, "r");
        if (!f) {
            continue;
        }
        if (!fgets(comm, sizeof(comm), f)) {
            fclose(f);
            continue;
        }
        fclose(f);
        trim_newline(comm);
        if (strcmp(comm, "mpvpaper") != 0) {
            continue;
        }

        if (!matches) {
            snprintf(cmd_path, sizeof(cmd_path), "/proc/%s/cmdline", entry->d_name);
            f = fopen(cmd_path, "rb");
            if (!f) {
                continue;
            }
            n = fread(data, 1, sizeof(data) - 1, f);
            fclose(f);
            data[n] = '\0';
            for (size_t i = 0; i < n;) {
                char *arg = &data[i];
                size_t len = strlen(arg);
                if (strcmp(arg, output) == 0 || strcmp(arg, "ALL") == 0 ||
                    strcmp(arg, "*") == 0) {
                    matches = 1;
                    break;
                }
                i += len + 1;
            }
        }
        if (matches && kill((pid_t)pid, SIGTERM) == 0) {
            stopped++;
        }
    }
    closedir(dir);
    if (stopped > 0) {
        append_log(paths, "stopped %d mpvpaper process(es) for output %s", stopped, output);
        usleep(120000);
    }
}

static void migrate_legacy_wallpaper_output_states(const Paths *paths) {
    char legacy_dir[PATH_MAX];
    DIR *dir;
    struct dirent *entry;

    if (dir_exists(paths->wallpaper_output_dir)) return;
    snprintf(legacy_dir, sizeof(legacy_dir), "%s/output-state",
             paths->cache_awww);
    dir = opendir(legacy_dir);
    if (!dir) return;
    mkdir_p(paths->wallpaper_output_dir);
    while ((entry = readdir(dir)) != NULL) {
        char source[PATH_MAX];
        char target[PATH_MAX];
        char output[256];
        char kind[16];
        char wallpaper[PATH_MAX];
        FILE *stream;
        unsigned long long hash;
        size_t length = strlen(entry->d_name);

        if (length <= 6 ||
            strcmp(entry->d_name + length - 6, ".state") != 0) {
            continue;
        }
        snprintf(source, sizeof(source), "%s/%s", legacy_dir,
                 entry->d_name);
        stream = fopen(source, "r");
        if (!stream) continue;
        if (!fgets(output, sizeof(output), stream) ||
            !fgets(kind, sizeof(kind), stream) ||
            !fgets(wallpaper, sizeof(wallpaper), stream)) {
            fclose(stream);
            continue;
        }
        fclose(stream);
        trim_newline(output);
        trim_newline(kind);
        trim_newline(wallpaper);
        if (!output_name_is_safe(output) || !regular_file_exists(wallpaper))
            continue;
        hash = fnv1a_hash(output);
        snprintf(target, sizeof(target), "%s/%016llx.state",
                 paths->wallpaper_output_dir, hash);
        write_text_file(target, "%s\n%s\n%s\n", output, kind, wallpaper);
    }
    closedir(dir);
}

static void remove_wallpaper_output_state(const Paths *paths,
                                          const char *output) {
    DIR *dir = opendir(paths->wallpaper_output_dir);
    struct dirent *entry;

    if (!dir) return;
    while ((entry = readdir(dir)) != NULL) {
        char state_path[PATH_MAX];
        char saved_output[256];
        size_t length = strlen(entry->d_name);

        if (length <= 6 ||
            strcmp(entry->d_name + length - 6, ".state") != 0) {
            continue;
        }
        snprintf(state_path, sizeof(state_path), "%s/%s",
                 paths->wallpaper_output_dir, entry->d_name);
        if (read_first_line(state_path, saved_output, sizeof(saved_output)) &&
            strcmp(saved_output, output) == 0) {
            unlink(state_path);
        }
    }
    closedir(dir);
}

static void write_wallpaper_output_state(const Paths *paths, const char *output,
                                         const char *kind, const char *wallpaper) {
    char state_file[PATH_MAX];
    unsigned long long hash;

    migrate_legacy_wallpaper_output_states(paths);
    mkdir_p(paths->wallpaper_output_dir);
    remove_wallpaper_output_state(paths, output);
    hash = fnv1a_hash(output);
    snprintf(state_file, sizeof(state_file), "%s/%016llx.state",
             paths->wallpaper_output_dir, hash);
    write_text_file(state_file, "%s\n%s\n%s\n", output, kind, wallpaper);
}

static void write_wallpaper_current_state(const Paths *paths,
                                          const char *output,
                                          const char *wallpaper) {
    write_text_file(paths->wallpaper_current_state, "%s\n%s\n", output,
                    wallpaper);
}

static void clear_wallpaper_output_states(const Paths *paths) {
    DIR *dir;
    struct dirent *entry;

    dir = opendir(paths->wallpaper_output_dir);
    if (!dir) {
        return;
    }
    while ((entry = readdir(dir)) != NULL) {
        char state_file[PATH_MAX];
        size_t length = strlen(entry->d_name);

        if (length <= 6 || strcmp(entry->d_name + length - 6, ".state") != 0) {
            continue;
        }
        snprintf(state_file, sizeof(state_file), "%s/%s",
                 paths->wallpaper_output_dir, entry->d_name);
        unlink(state_file);
    }
    closedir(dir);
}

static int detect_current_wallpaper(const Paths *paths, char *out, size_t out_size) {
    char cmd_out[PATH_MAX];
    char normalized[PATH_MAX];
    char cmd[1024];

    if (current_mpvpaper_path(out, out_size)) {
        return 1;
    }

    snprintf(cmd, sizeof(cmd), "awww query 2>/dev/null | awk -F'image: ' '/image:/ {print $2; exit}'");
    if (capture_command(cmd, cmd_out, sizeof(cmd_out)) && regular_file_exists(cmd_out)) {
        normalize_existing_path(cmd_out, out, out_size);
        return 1;
    }

    snprintf(cmd_out, sizeof(cmd_out), "%s/current-wallpaper.path", paths->cache_awww);
    if (read_first_line(cmd_out, out, out_size) && regular_file_exists(out)) {
        normalize_existing_path(out, normalized, sizeof(normalized));
        copy_string(out, out_size, normalized);
        return 1;
    }
    return 0;
}

static int is_video_file(const char *path) {
    char *qpath;
    char cmd[PATH_MAX + 128];
    char mime[256];
    int result;

    qpath = shell_quote(path);
    if (!qpath) {
        return 0;
    }
    snprintf(cmd, sizeof(cmd), "file --mime-type -b %s 2>/dev/null", qpath);
    result = capture_command(cmd, mime, sizeof(mime)) && starts_with(mime, "video/");
    free(qpath);
    return result;
}

static int extract_video_thumbnail_to(const char *video, const char *thumb_path) {
    char *qvideo;
    char *qthumb;
    char cmd[PATH_MAX * 2 + 512];

    ensure_parent_dir(thumb_path);
    qvideo = shell_quote(video);
    qthumb = shell_quote(thumb_path);
    if (!qvideo || !qthumb) {
        free(qvideo);
        free(qthumb);
        return 0;
    }
    snprintf(cmd, sizeof(cmd), "ffmpeg -y -ss 00:00:01 -i %s -vframes 1 %s >/dev/null 2>&1", qvideo, qthumb);
    if (!run_shell(cmd)) {
        snprintf(cmd, sizeof(cmd), "ffmpeg -y -i %s -vframes 1 %s >/dev/null 2>&1", qvideo, qthumb);
        run_shell(cmd);
    }
    free(qvideo);
    free(qthumb);
    return regular_file_exists(thumb_path);
}

static int cached_file_is_fresh(const char *cache_path, const char *source_path) {
    struct stat cache_stat;
    struct stat source_stat;
    return stat(cache_path, &cache_stat) == 0 && S_ISREG(cache_stat.st_mode) &&
           cache_stat.st_size > 0 && stat(source_path, &source_stat) == 0 &&
           cache_stat.st_mtime >= source_stat.st_mtime;
}

static int selector_cached_preview(const Paths *paths, const char *asset,
                                   char *preview, size_t preview_size) {
    char catalog[PATH_MAX];
    char *qcatalog;
    char *qasset;
    char command[PATH_MAX * 3 + 512];

    snprintf(catalog, sizeof(catalog), "%s/catalog.json",
             paths->selector_cache_dir);
    if (!regular_file_exists(catalog) || !command_exists("jq")) return 0;
    qcatalog = shell_quote(catalog);
    qasset = shell_quote(asset);
    if (!qcatalog || !qasset) {
        free(qcatalog);
        free(qasset);
        return 0;
    }
    snprintf(command, sizeof(command),
             "jq -r --arg p %s '[.[] | select(.path == $p and "
             "(.preview // \"\") != \"\")] | first | .preview // empty' "
             "%s 2>/dev/null",
             qasset, qcatalog);
    free(qcatalog);
    free(qasset);
    if (!capture_command(command, preview, preview_size) ||
        !regular_file_exists(preview)) {
        preview[0] = '\0';
        return 0;
    }
    return 1;
}

static int palette_sample_source(const Paths *paths, const char *asset,
                                 const char *kind, char *sample,
                                 size_t sample_size) {
    unsigned long long hash;

    if ((kind && strcmp(kind, "live") == 0) || is_video_file(asset)) {
        if (selector_cached_preview(paths, asset, sample, sample_size)) {
            return 1;
        }
        hash = fnv1a_hash(asset);
        snprintf(sample, sample_size, "%s/palette-video-%016llx.png",
                 paths->cache_awww, hash);
        if (cached_file_is_fresh(sample, asset)) return 1;
        if (!command_exists("ffmpeg")) return 0;
        return extract_video_thumbnail_to(asset, sample);
    }
    copy_string(sample, sample_size, asset);
    return regular_file_exists(sample);
}

static int read_output_geometries(OutputGeometry *outputs, size_t *count) {
    const char *fixture_path = getenv("ANTO426_WALLPAPER_MONITORS_FILE");
    FILE *stream = NULL;
    int pipe_stream = 0;
    char line[2048];

    *count = 0;
    if (fixture_path && fixture_path[0]) {
        stream = fopen(fixture_path, "r");
    } else if (command_exists("hyprctl") && command_exists("jq")) {
        stream = popen(
            "hyprctl monitors -j 2>/dev/null | jq -r '.[] | "
            "select((.disabled // false) == false) | [.name, (.width // 0), "
            "(.height // 0), (.scale // 1)] | @tsv' 2>/dev/null",
            "r");
        pipe_stream = stream != NULL;
    }
    if (!stream) return 0;

    while (*count < MAX_WALLPAPER_OUTPUTS &&
           fgets(line, sizeof(line), stream)) {
        OutputGeometry geometry;
        char scale_text[64];
        char *end = NULL;
        memset(&geometry, 0, sizeof(geometry));
        if (sscanf(line, "%255[^\t]\t%d\t%d\t%63[^\t\r\n]",
                   geometry.output, &geometry.width, &geometry.height,
                   scale_text) != 4 || !output_name_is_safe(geometry.output)) {
            continue;
        }
        geometry.scale = strtod(scale_text, &end);
        if (!end || (*end != '\0' && *end != '\n' && *end != '\r') ||
            geometry.scale <= 0.0) {
            geometry.scale = 1.0;
        }
        outputs[(*count)++] = geometry;
    }
    if (pipe_stream) {
        pclose(stream);
    } else {
        fclose(stream);
    }
    return *count > 0;
}

static size_t load_wallpaper_states_from_dir(const char *directory,
                                             WallpaperAssignment *states,
                                             size_t capacity) {
    DIR *dir = opendir(directory);
    struct dirent *entry;
    size_t count = 0;

    if (!dir) return 0;
    while (count < capacity && (entry = readdir(dir)) != NULL) {
        char state_path[PATH_MAX];
        FILE *state_file;
        size_t length = strlen(entry->d_name);
        WallpaperAssignment state;

        if (length <= 6 ||
            strcmp(entry->d_name + length - 6, ".state") != 0) {
            continue;
        }
        snprintf(state_path, sizeof(state_path), "%s/%s", directory,
                 entry->d_name);
        state_file = fopen(state_path, "r");
        if (!state_file) continue;
        memset(&state, 0, sizeof(state));
        if (!fgets(state.output, sizeof(state.output), state_file) ||
            !fgets(state.kind, sizeof(state.kind), state_file) ||
            !fgets(state.asset, sizeof(state.asset), state_file)) {
            fclose(state_file);
            continue;
        }
        fclose(state_file);
        trim_newline(state.output);
        trim_newline(state.kind);
        trim_newline(state.asset);
        if (!output_name_is_safe(state.output) ||
            !regular_file_exists(state.asset)) {
            continue;
        }
        {
            char normalized[PATH_MAX];
            normalize_existing_path(state.asset, normalized,
                                    sizeof(normalized));
            copy_string(state.asset, sizeof(state.asset), normalized);
        }
        states[count++] = state;
    }
    closedir(dir);
    qsort(states, count, sizeof(states[0]), compare_assignment_output);
    return count;
}

static const WallpaperAssignment *wallpaper_state_for_output(
    const WallpaperAssignment *states, size_t count, const char *output) {
    const WallpaperAssignment *all_state = NULL;

    for (size_t index = 0; index < count; index++) {
        if (strcmp(states[index].output, output) == 0) return &states[index];
        if (strcmp(states[index].output, "ALL") == 0) all_state = &states[index];
    }
    return all_state;
}

static int collect_active_wallpaper_assignments(
    const Paths *paths, const char *fallback_asset,
    WallpaperPaletteAggregate *aggregate) {
    WallpaperAssignment states[MAX_WALLPAPER_OUTPUTS];
    OutputGeometry outputs[MAX_WALLPAPER_OUTPUTS];
    char legacy_dir[PATH_MAX];
    size_t state_count;
    size_t output_count = 0;

    memset(aggregate, 0, sizeof(*aggregate));
    state_count = load_wallpaper_states_from_dir(paths->wallpaper_output_dir,
                                                 states,
                                                 MAX_WALLPAPER_OUTPUTS);
    if (state_count == 0) {
        snprintf(legacy_dir, sizeof(legacy_dir), "%s/output-state",
                 paths->cache_awww);
        state_count = load_wallpaper_states_from_dir(
            legacy_dir, states, MAX_WALLPAPER_OUTPUTS);
    }

    if (read_output_geometries(outputs, &output_count)) {
        for (size_t index = 0; index < output_count; index++) {
            const WallpaperAssignment *state = wallpaper_state_for_output(
                states, state_count, outputs[index].output);
            WallpaperAssignment *assignment;
            double logical_width;
            double logical_height;

            if (!state || aggregate->assignment_count >= MAX_WALLPAPER_OUTPUTS)
                continue;
            assignment =
                &aggregate->assignments[aggregate->assignment_count++];
            *assignment = *state;
            copy_string(assignment->output, sizeof(assignment->output),
                        outputs[index].output);
            logical_width = (double)outputs[index].width / outputs[index].scale;
            logical_height =
                (double)outputs[index].height / outputs[index].scale;
            assignment->logical_area = logical_width > 0.0 &&
                                               logical_height > 0.0
                                           ? logical_width * logical_height
                                           : 1.0;
        }
    } else if (state_count > 0) {
        int has_specific = 0;
        for (size_t index = 0; index < state_count; index++) {
            if (strcmp(states[index].output, "ALL") != 0) {
                has_specific = 1;
                break;
            }
        }
        for (size_t index = 0; index < state_count &&
                               aggregate->assignment_count <
                                   MAX_WALLPAPER_OUTPUTS;
             index++) {
            if (has_specific && strcmp(states[index].output, "ALL") == 0)
                continue;
            states[index].logical_area = 1.0;
            aggregate->assignments[aggregate->assignment_count++] =
                states[index];
        }
    }

    if (aggregate->assignment_count == 0 && fallback_asset &&
        regular_file_exists(fallback_asset)) {
        WallpaperAssignment *fallback = &aggregate->assignments[0];
        memset(fallback, 0, sizeof(*fallback));
        copy_string(fallback->output, sizeof(fallback->output), "ALL");
        copy_string(fallback->kind, sizeof(fallback->kind),
                    is_video_file(fallback_asset) ? "live" : "image");
        normalize_existing_path(fallback_asset, fallback->asset,
                                sizeof(fallback->asset));
        fallback->logical_area = 1.0;
        aggregate->assignment_count = 1;
    }
    return aggregate->assignment_count > 0;
}

static int sample_active_wallpapers(const Paths *paths,
                                    WallpaperPaletteAggregate *aggregate) {
    for (size_t index = 0; index < aggregate->assignment_count; index++) {
        WallpaperAssignment *assignment = &aggregate->assignments[index];
        int reused = 0;

        for (size_t previous = 0; previous < index; previous++) {
            WallpaperAssignment *sampled = &aggregate->assignments[previous];
            if (sampled->sampled &&
                strcmp(sampled->asset, assignment->asset) == 0) {
                assignment->avg_r = sampled->avg_r;
                assignment->avg_g = sampled->avg_g;
                assignment->avg_b = sampled->avg_b;
                assignment->accent_r = sampled->accent_r;
                assignment->accent_g = sampled->accent_g;
                assignment->accent_b = sampled->accent_b;
                copy_string(assignment->sample_path,
                            sizeof(assignment->sample_path),
                            sampled->sample_path);
                assignment->sampled = 1;
                reused = 1;
                break;
            }
        }
        if (reused) continue;
        if (!palette_sample_source(paths, assignment->asset, assignment->kind,
                                   assignment->sample_path,
                                   sizeof(assignment->sample_path)) ||
            !extract_average_rgb(assignment->sample_path, &assignment->avg_r,
                                 &assignment->avg_g,
                                 &assignment->avg_b)) {
            return 0;
        }
        if (!extract_vibrant_rgb(assignment->sample_path,
                                 &assignment->accent_r,
                                 &assignment->accent_g,
                                 &assignment->accent_b)) {
            assignment->accent_r = assignment->avg_r;
            assignment->accent_g = assignment->avg_g;
            assignment->accent_b = assignment->avg_b;
        }
        assignment->sampled = 1;
    }
    return aggregate_wallpaper_samples(aggregate);
}

static Canvas detect_canvas_size(void) {
    Canvas canvas = {2560, 1600, "2560x1600"};
    char out[128];
    const char *json_cmd =
        "hyprctl monitors -j 2>/dev/null | "
        "jq -r '([.[] | select(.focused == true)][0] // (sort_by(.width * .height) | last)) | "
        "select(.width and .height) | \"\\(.width)x\\(.height)\"' 2>/dev/null";
    const char *text_cmd =
        "hyprctl monitors 2>/dev/null | awk '"
        "/^[[:space:]]*[0-9]+x[0-9]+@/ {"
        "split($1, mode, \"@\"); split(mode[1], size, \"x\"); area = size[1] * size[2];"
        "if (area > best_area) { best_area = area; best = mode[1] }"
        "} END { if (best != \"\") print best }'";

    if ((command_exists("hyprctl") && command_exists("jq") && capture_command(json_cmd, out, sizeof(out))) ||
        (command_exists("hyprctl") && capture_command(text_cmd, out, sizeof(out)))) {
        int w = 0;
        int h = 0;
        if (sscanf(out, "%dx%d", &w, &h) == 2 && w > 0 && h > 0) {
            canvas.width = w;
            canvas.height = h;
            snprintf(canvas.text, sizeof(canvas.text), "%dx%d", w, h);
        }
    }
    return canvas;
}

static int make_cover_image(const char *src, const char *size, const char *dst, const char *kind, int quality) {
    char *qsrc;
    char *qresize;
    char *qextent;
    char *qdst;
    char dst_spec[PATH_MAX + 16];
    char cmd[PATH_MAX * 4 + 1024];
    int ok;

    ensure_parent_dir(dst);
    qsrc = shell_quote(src);
    {
        char resize[128];
        snprintf(resize, sizeof(resize), "%s^", size);
        qresize = shell_quote(resize);
    }
    qextent = shell_quote(size);
    if (kind && (strcmp(kind, "jpg") == 0 || strcmp(kind, "jpeg") == 0)) {
        snprintf(dst_spec, sizeof(dst_spec), "jpg:%s", dst);
    } else {
        snprintf(dst_spec, sizeof(dst_spec), "%s", dst);
    }
    qdst = shell_quote(dst_spec);

    if (!qsrc || !qresize || !qextent || !qdst) {
        free(qsrc);
        free(qresize);
        free(qextent);
        free(qdst);
        return 0;
    }

    if (kind && (strcmp(kind, "jpg") == 0 || strcmp(kind, "jpeg") == 0)) {
        snprintf(cmd, sizeof(cmd),
                 "magick %s -auto-orient -resize %s -gravity center -extent %s -strip -colorspace sRGB "
                 "-sampling-factor 4:2:0 -interlace none -quality %d %s",
                 qsrc, qresize, qextent, quality, qdst);
    } else {
        snprintf(cmd, sizeof(cmd),
                 "magick %s -auto-orient -resize %s -gravity center -extent %s -strip -colorspace sRGB %s",
                 qsrc, qresize, qextent, qdst);
    }
    ok = run_shell(cmd);
    free(qsrc);
    free(qresize);
    free(qextent);
    free(qdst);
    return ok;
}

static int extract_average_rgb(const char *src, int *r, int *g, int *b) {
    char *qsrc;
    char cmd[PATH_MAX + 512];
    char out[256];
    int ok = 0;

    qsrc = shell_quote(src);
    if (!qsrc) {
        return 0;
    }
    snprintf(cmd, sizeof(cmd),
             "magick %s -auto-orient -alpha off -colorspace sRGB -resize '96x96!' -filter triangle "
             "-resize '1x1!' -format '%%[fx:int(255*r)] %%[fx:int(255*g)] %%[fx:int(255*b)]\\n' info:",
             qsrc);
    if (capture_command(cmd, out, sizeof(out)) && sscanf(out, "%d %d %d", r, g, b) == 3) {
        ok = 1;
    }
    free(qsrc);
    return ok;
}

static int hex_digit_value(char c) {
    c = (char)tolower((unsigned char)c);
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    return -1;
}

static int parse_histogram_rgb(const char *line, int *r, int *g, int *b) {
    const char *hash = strchr(line, '#');
    if (hash) {
        int values[6];
        int ok = 1;
        for (int i = 0; i < 6; i++) {
            values[i] = hex_digit_value(hash[i + 1]);
            if (values[i] < 0) {
                ok = 0;
                break;
            }
        }
        if (ok) {
            *r = values[0] * 16 + values[1];
            *g = values[2] * 16 + values[3];
            *b = values[4] * 16 + values[5];
            return 1;
        }
    }

    for (const char *p = line; *p; p++) {
        if (*p == '(') {
            double rr = 0;
            double gg = 0;
            double bb = 0;
            if (sscanf(p + 1, "%lf,%lf,%lf", &rr, &gg, &bb) == 3) {
                *r = (int)(rr + 0.5);
                *g = (int)(gg + 0.5);
                *b = (int)(bb + 0.5);
                return 1;
            }
        }
    }
    return 0;
}

static double color_brightness(double r, double g, double b) {
    return (299.0 * r + 587.0 * g + 114.0 * b) / 1000.0;
}

static int extract_vibrant_rgb(const char *src, int *out_r, int *out_g, int *out_b) {
    char *qsrc;
    char cmd[PATH_MAX + 768];
    FILE *fp;
    char line[4096];
    int found = 0;
    int fallback_found = 0;
    double best_score = -1.0;
    double best_fallback_score = -1.0;
    int best_r = 0, best_g = 0, best_b = 0;
    int fallback_r = 0, fallback_g = 0, fallback_b = 0;

    qsrc = shell_quote(src);
    if (!qsrc) {
        return 0;
    }
    snprintf(cmd, sizeof(cmd),
             "magick %s -auto-orient -alpha off -resize '320x320^' -gravity center -extent '320x320' "
             "-colorspace sRGB +dither -colors 32 -depth 8 -format %%c histogram:info:-",
             qsrc);
    free(qsrc);

    fp = popen(cmd, "r");
    if (!fp) {
        return 0;
    }

    while (fgets(line, sizeof(line), fp)) {
        char *trimmed = line;
        long count;
        int r, g, b;
        int maxc, minc, chroma;
        double sat;
        double bright;
        double balance;
        double fallback_score;
        double score;

        while (*trimmed && isspace((unsigned char)*trimmed)) {
            trimmed++;
        }
        count = strtol(trimmed, NULL, 10);
        if (count <= 0 || !parse_histogram_rgb(trimmed, &r, &g, &b)) {
            continue;
        }

        maxc = r > g ? r : g;
        if (b > maxc) {
            maxc = b;
        }
        minc = r < g ? r : g;
        if (b < minc) {
            minc = b;
        }
        chroma = maxc - minc;
        sat = maxc > 0 ? (double)chroma / (double)maxc : 0.0;
        bright = color_brightness(r, g, b);
        balance = 1.0 - fabs(bright - 145.0) / 145.0;
        if (balance < 0.0) {
            balance = 0.0;
        }

        fallback_score = log((double)count + 1.0) * (0.5 + balance);
        if (bright >= 34.0 && bright <= 236.0 && fallback_score > best_fallback_score) {
            best_fallback_score = fallback_score;
            fallback_r = r;
            fallback_g = g;
            fallback_b = b;
            fallback_found = 1;
        }

        if (bright < 42.0 || bright > 226.0 || sat < 0.13) {
            continue;
        }

        score = log((double)count + 1.0) * (0.35 + sat * 2.9) * (0.55 + balance) * (1.0 + (double)chroma / 255.0);
        if (sat < 0.20) {
            score *= 0.45;
        }
        if (score > best_score) {
            best_score = score;
            best_r = r;
            best_g = g;
            best_b = b;
            found = 1;
        }
    }
    pclose(fp);

    if (found) {
        *out_r = best_r;
        *out_g = best_g;
        *out_b = best_b;
        return 1;
    }
    if (fallback_found) {
        *out_r = fallback_r;
        *out_g = fallback_g;
        *out_b = fallback_b;
        return 1;
    }
    return extract_average_rgb(src, out_r, out_g, out_b);
}

static int clamp_channel(double v) {
    if (v < 0.0) {
        return 0;
    }
    if (v > 255.0) {
        return 255;
    }
    return (int)(v + 0.5);
}

static void set_color(Color *color, double r, double g, double b) {
    color->r = clamp_channel(r);
    color->g = clamp_channel(g);
    color->b = clamp_channel(b);
    snprintf(color->hex, sizeof(color->hex), "#%02x%02x%02x", color->r, color->g, color->b);
}

static double mix(double a, double b, double ratio) {
    return a * (1.0 - ratio) + b * ratio;
}

static double max3(double a, double b, double c) {
    double m = a > b ? a : b;
    return m > c ? m : c;
}

static double min3(double a, double b, double c) {
    double m = a < b ? a : b;
    return m < c ? m : c;
}

static double linear_rgb(double v) {
    v /= 255.0;
    return v <= 0.03928 ? v / 12.92 : pow((v + 0.055) / 1.055, 2.4);
}

static double luminance(double r, double g, double b) {
    return 0.2126 * linear_rgb(r) + 0.7152 * linear_rgb(g) + 0.0722 * linear_rgb(b);
}

static double contrast_ratio(double l1, double l2) {
    if (l1 < l2) {
        double tmp = l1;
        l1 = l2;
        l2 = tmp;
    }
    return (l1 + 0.05) / (l2 + 0.05);
}

static void rofi_rgba(const Color *color, int alpha_percent, char *out, size_t out_size) {
    snprintf(out, out_size, "rgba ( %d, %d, %d, %d %% )", color->r, color->g, color->b, alpha_percent);
}

static void css_rgba(const Color *color, const char *alpha, char *out, size_t out_size) {
    snprintf(out, out_size, "rgba(%d, %d, %d, %s)", color->r, color->g, color->b, alpha);
}

static const char *hex_no_hash(const Color *color) {
    return color->hex + 1;
}

static void generate_palette(int r, int g, int b, int ar_in, int ag_in, int ab_in, Palette *p) {
    double ar = ar_in;
    double ag = ag_in;
    double ab = ab_in;
    double accent_chroma;
    double accent_brightness;
    double bg_r, bg_g, bg_b;
    double accent_r, accent_g, accent_b;
    double surface_r, surface_g, surface_b;
    double select_r, select_g, select_b;
    double border_r, border_g, border_b;
    double bg_luma;
    double white_luma = luminance(246, 247, 251);
    double black_luma = luminance(17, 17, 27);
    double accent_luma;

    memset(p, 0, sizeof(*p));

    accent_chroma = max3(ar, ag, ab) - min3(ar, ag, ab);
    if (accent_chroma < 24.0) {
        ar = clamp_channel(r * 0.28 + 120 * 0.72);
        ag = clamp_channel(g * 0.28 + 168 * 0.72);
        ab = clamp_channel(b * 0.28 + 228 * 0.72);
    }

    accent_brightness = color_brightness(ar, ag, ab);
    if (accent_brightness < 92.0) {
        ar = mix(ar, 255, 0.40);
        ag = mix(ag, 255, 0.40);
        ab = mix(ab, 255, 0.40);
    } else if (accent_brightness > 205.0) {
        ar = mix(ar, 0, 0.18);
        ag = mix(ag, 0, 0.18);
        ab = mix(ab, 0, 0.18);
    } else {
        ar = mix(ar, 255, 0.10);
        ag = mix(ag, 255, 0.10);
        ab = mix(ab, 255, 0.10);
    }

    bg_r = clamp_channel(r * 0.26 + ar * 0.07 + 8);
    bg_g = clamp_channel(g * 0.26 + ag * 0.07 + 8);
    bg_b = clamp_channel(b * 0.26 + ab * 0.07 + 8);

    if (color_brightness(bg_r, bg_g, bg_b) > 112.0) {
        bg_r = mix(bg_r, 0, 0.18);
        bg_g = mix(bg_g, 0, 0.18);
        bg_b = mix(bg_b, 0, 0.18);
    } else if (color_brightness(bg_r, bg_g, bg_b) < 20.0) {
        bg_r = mix(bg_r, 255, 0.06);
        bg_g = mix(bg_g, 255, 0.06);
        bg_b = mix(bg_b, 255, 0.06);
    }

    accent_r = clamp_channel(ar);
    accent_g = clamp_channel(ag);
    accent_b = clamp_channel(ab);

    surface_r = clamp_channel(bg_r * 0.68 + r * 0.14 + accent_r * 0.08 + 255 * 0.10);
    surface_g = clamp_channel(bg_g * 0.68 + g * 0.14 + accent_g * 0.08 + 255 * 0.10);
    surface_b = clamp_channel(bg_b * 0.68 + b * 0.14 + accent_b * 0.08 + 255 * 0.10);

    select_r = clamp_channel(bg_r * 0.48 + accent_r * 0.42 + 255 * 0.10);
    select_g = clamp_channel(bg_g * 0.48 + accent_g * 0.42 + 255 * 0.10);
    select_b = clamp_channel(bg_b * 0.48 + accent_b * 0.42 + 255 * 0.10);

    border_r = clamp_channel(bg_r * 0.40 + accent_r * 0.38 + 255 * 0.22);
    border_g = clamp_channel(bg_g * 0.40 + accent_g * 0.38 + 255 * 0.22);
    border_b = clamp_channel(bg_b * 0.40 + accent_b * 0.38 + 255 * 0.22);

    bg_luma = luminance(bg_r, bg_g, bg_b);
    if (contrast_ratio(bg_luma, white_luma) >= contrast_ratio(bg_luma, black_luma)) {
        set_color(&p->foreground, 246, 247, 251);
        set_color(&p->muted, mix(bg_r, 246, 0.62), mix(bg_g, 247, 0.62), mix(bg_b, 251, 0.62));
    } else {
        set_color(&p->foreground, 17, 17, 27);
        set_color(&p->muted, mix(bg_r, 17, 0.58), mix(bg_g, 17, 0.58), mix(bg_b, 27, 0.58));
    }

    set_color(&p->background, bg_r, bg_g, bg_b);
    set_color(&p->surface, surface_r, surface_g, surface_b);
    set_color(&p->select, select_r, select_g, select_b);
    set_color(&p->accent, accent_r, accent_g, accent_b);
    set_color(&p->border, border_r, border_g, border_b);
    set_color(&p->base, mix(bg_r, surface_r, 0.24), mix(bg_g, surface_g, 0.24), mix(bg_b, surface_b, 0.24));
    set_color(&p->base_alt, mix(bg_r, surface_r, 0.62), mix(bg_g, surface_g, 0.62), mix(bg_b, surface_b, 0.62));
    set_color(&p->titlebar, mix(surface_r, accent_r, 0.08), mix(surface_g, accent_g, 0.08), mix(surface_b, accent_b, 0.08));
    set_color(&p->titlebar_backdrop, mix(bg_r, surface_r, 0.78), mix(bg_g, surface_g, 0.78), mix(bg_b, surface_b, 0.78));
    set_color(&p->popover, mix(bg_r, surface_r, 0.78), mix(bg_g, surface_g, 0.78), mix(bg_b, surface_b, 0.78));

    accent_luma = luminance(accent_r, accent_g, accent_b);
    if (contrast_ratio(accent_luma, black_luma) >= contrast_ratio(accent_luma, white_luma)) {
        set_color(&p->selected_fg, 17, 17, 27);
    } else {
        set_color(&p->selected_fg, 246, 247, 251);
    }

    set_color(&p->red, ar * 0.15 + 243 * 0.85, ag * 0.15 + 139 * 0.85, ab * 0.15 + 168 * 0.85);
    set_color(&p->orange, ar * 0.12 + 250 * 0.88, ag * 0.12 + 179 * 0.88, ab * 0.10 + 135 * 0.90);
    set_color(&p->yellow, ar * 0.08 + 249 * 0.92, ag * 0.08 + 226 * 0.92, ab * 0.05 + 175 * 0.95);
    set_color(&p->green, ar * 0.12 + 166 * 0.88, ag * 0.15 + 227 * 0.85, ab * 0.12 + 161 * 0.88);
    set_color(&p->pink, ar * 0.15 + 245 * 0.85, ag * 0.15 + 194 * 0.85, ab * 0.15 + 231 * 0.85);
    set_color(&p->purple, ar * 0.15 + 203 * 0.85, ag * 0.12 + 166 * 0.88, ab * 0.15 + 247 * 0.85);
    set_color(&p->gray, r * 0.20 + 108 * 0.80, g * 0.20 + 112 * 0.80, b * 0.20 + 134 * 0.80);

    rofi_rgba(&p->background, 62, p->panel_bg, sizeof(p->panel_bg));
    rofi_rgba(&p->background, 78, p->panel_bg_hover, sizeof(p->panel_bg_hover));
    rofi_rgba(&p->background, 28, p->overlay_bg, sizeof(p->overlay_bg));
    rofi_rgba(&p->surface, 18, p->item_bg, sizeof(p->item_bg));
    rofi_rgba(&p->select, 30, p->item_bg_hover, sizeof(p->item_bg_hover));
    rofi_rgba(&p->select, 42, p->item_bg_active, sizeof(p->item_bg_active));
    rofi_rgba(&p->accent, 22, p->accent_soft, sizeof(p->accent_soft));
    rofi_rgba(&p->accent, 42, p->accent_strong, sizeof(p->accent_strong));
    rofi_rgba(&p->border, 16, p->border_soft, sizeof(p->border_soft));
    rofi_rgba(&p->border, 34, p->border_medium, sizeof(p->border_medium));
    rofi_rgba(&p->background, 18, p->shadow_soft, sizeof(p->shadow_soft));
    rofi_rgba(&p->background, 30, p->shadow_medium, sizeof(p->shadow_medium));

    copy_string(p->background_alpha, sizeof(p->background_alpha), p->panel_bg);
    copy_string(p->surface_alpha, sizeof(p->surface_alpha), p->item_bg);
    copy_string(p->select_alpha, sizeof(p->select_alpha), p->item_bg_active);
    copy_string(p->accent_alpha, sizeof(p->accent_alpha), p->accent_strong);
    copy_string(p->border_alpha, sizeof(p->border_alpha), p->border_medium);
}

static void write_color_files(const Paths *paths, const Palette *p, const char *current_wallpaper_path) {
    char css_panel_bg[80], css_panel_bg_hover[80], css_overlay_bg[80];
    char css_item_bg[80], css_item_bg_hover[80], css_item_bg_active[80];
    char css_accent_soft[80], css_accent_strong[80], css_border_soft[80], css_border_medium[80];
    char css_shadow_soft[80], css_shadow_medium[80];
    char colors_css[PATH_MAX], colors_rasi[PATH_MAX], colors_sh[PATH_MAX];

    snprintf(colors_css, sizeof(colors_css), "%s/colors.css", paths->colors_dir);
    snprintf(colors_rasi, sizeof(colors_rasi), "%s/colors.rasi", paths->colors_dir);
    snprintf(colors_sh, sizeof(colors_sh), "%s/colors.sh", paths->colors_dir);

    css_rgba(&p->background, "0.62", css_panel_bg, sizeof(css_panel_bg));
    css_rgba(&p->background, "0.78", css_panel_bg_hover, sizeof(css_panel_bg_hover));
    css_rgba(&p->background, "0.28", css_overlay_bg, sizeof(css_overlay_bg));
    css_rgba(&p->surface, "0.18", css_item_bg, sizeof(css_item_bg));
    css_rgba(&p->select, "0.30", css_item_bg_hover, sizeof(css_item_bg_hover));
    css_rgba(&p->select, "0.42", css_item_bg_active, sizeof(css_item_bg_active));
    css_rgba(&p->accent, "0.22", css_accent_soft, sizeof(css_accent_soft));
    css_rgba(&p->accent, "0.42", css_accent_strong, sizeof(css_accent_strong));
    css_rgba(&p->border, "0.16", css_border_soft, sizeof(css_border_soft));
    css_rgba(&p->border, "0.34", css_border_medium, sizeof(css_border_medium));
    css_rgba(&p->background, "0.18", css_shadow_soft, sizeof(css_shadow_soft));
    css_rgba(&p->background, "0.30", css_shadow_medium, sizeof(css_shadow_medium));

    write_text_file(colors_css,
                    "@define-color background %s;\n"
                    "@define-color surface    %s;\n"
                    "@define-color base       %s;\n"
                    "@define-color base-alt   %s;\n"
                    "@define-color titlebar   %s;\n"
                    "@define-color titlebar-backdrop %s;\n"
                    "@define-color popover    %s;\n"
                    "@define-color foreground %s;\n"
                    "@define-color muted      %s;\n"
                    "@define-color select     %s;\n"
                    "@define-color accent     %s;\n"
                    "@define-color selected-fg %s;\n"
                    "@define-color border     %s;\n\n"
                    "@define-color panel-bg         %s;\n"
                    "@define-color panel-bg-hover   %s;\n"
                    "@define-color overlay-bg       %s;\n"
                    "@define-color item-bg          %s;\n"
                    "@define-color item-bg-hover    %s;\n"
                    "@define-color item-bg-active   %s;\n"
                    "@define-color accent-soft      %s;\n"
                    "@define-color accent-strong    %s;\n"
                    "@define-color border-soft      %s;\n"
                    "@define-color border-medium    %s;\n"
                    "@define-color shadow-soft      %s;\n"
                    "@define-color shadow-medium    %s;\n\n"
                    "@define-color background-alpha @panel-bg;\n"
                    "@define-color surface-alpha    @item-bg;\n"
                    "@define-color select-alpha     @item-bg-active;\n"
                    "@define-color accent-alpha     @accent-strong;\n"
                    "@define-color border-alpha     @border-medium;\n\n"
                    "@define-color pink       %s;\n"
                    "@define-color purple     %s;\n"
                    "@define-color red        %s;\n"
                    "@define-color orange     %s;\n"
                    "@define-color yellow     %s;\n"
                    "@define-color green      %s;\n"
                    "@define-color blue       @accent;\n"
                    "@define-color cyan       @muted;\n"
                    "@define-color gray       %s;\n",
                    p->background.hex, p->surface.hex, p->base.hex, p->base_alt.hex, p->titlebar.hex,
                    p->titlebar_backdrop.hex, p->popover.hex, p->foreground.hex, p->muted.hex, p->select.hex,
                    p->accent.hex, p->selected_fg.hex, p->border.hex, css_panel_bg, css_panel_bg_hover,
                    css_overlay_bg, css_item_bg, css_item_bg_hover, css_item_bg_active, css_accent_soft,
                    css_accent_strong, css_border_soft, css_border_medium, css_shadow_soft, css_shadow_medium,
                    p->pink.hex, p->purple.hex, p->red.hex, p->orange.hex, p->yellow.hex, p->green.hex, p->gray.hex);

    write_text_file(colors_rasi,
                    "* {\n"
                    "    background: %s;\n"
                    "    surface: %s;\n"
                    "    base: %s;\n"
                    "    base-alt: %s;\n"
                    "    titlebar: %s;\n"
                    "    titlebar-backdrop: %s;\n"
                    "    popover: %s;\n"
                    "    panel-bg: %s;\n"
                    "    panel-bg-hover: %s;\n"
                    "    overlay-bg: %s;\n"
                    "    item-bg: %s;\n"
                    "    item-bg-hover: %s;\n"
                    "    item-bg-active: %s;\n"
                    "    accent-soft: %s;\n"
                    "    accent-strong: %s;\n"
                    "    border-soft: %s;\n"
                    "    border-medium: %s;\n"
                    "    shadow-soft: %s;\n"
                    "    shadow-medium: %s;\n"
                    "    background-alpha: %s;\n"
                    "    surface-alpha: %s;\n"
                    "    foreground: %s;\n"
                    "    muted: %s;\n"
                    "    select: %s;\n"
                    "    select-alpha: %s;\n\n"
                    "    accent: %s;\n"
                    "    selected-fg: %s;\n"
                    "    accent-alpha: %s;\n"
                    "    border: %s;\n"
                    "    border-alpha: %s;\n"
                    "    pink: %s;\n"
                    "    purple: %s;\n"
                    "    red: %s;\n"
                    "    orange: %s;\n"
                    "    yellow: %s;\n"
                    "    green: %s;\n"
                    "    blue: @accent;\n"
                    "    cyan: @muted;\n"
                    "    gray: %s;\n\n"
                    "    active-background: @select;\n"
                    "    urgent-background: @red;\n"
                    "    urgent-foreground: @background;\n"
                    "    selected-background: @accent;\n"
                    "    selected-urgent-background: @urgent-background;\n"
                    "    selected-active-background: @active-background;\n"
                    "    bordercolor: @border;\n"
                    "}\n",
                    p->background.hex, p->surface.hex, p->base.hex, p->base_alt.hex, p->titlebar.hex,
                    p->titlebar_backdrop.hex, p->popover.hex, p->panel_bg, p->panel_bg_hover, p->overlay_bg,
                    p->item_bg, p->item_bg_hover, p->item_bg_active, p->accent_soft, p->accent_strong,
                    p->border_soft, p->border_medium, p->shadow_soft, p->shadow_medium, p->background_alpha,
                    p->surface_alpha, p->foreground.hex, p->muted.hex, p->select.hex, p->select_alpha,
                    p->accent.hex, p->selected_fg.hex, p->accent_alpha, p->border.hex, p->border_alpha,
                    p->pink.hex, p->purple.hex, p->red.hex, p->orange.hex, p->yellow.hex, p->green.hex, p->gray.hex);

    write_text_file(colors_sh,
                    "# Generated by wallpaper_core.c\n"
                    "export ANTO426_BACKGROUND=\"%s\"\n"
                    "export ANTO426_SURFACE=\"%s\"\n"
                    "export ANTO426_BASE=\"%s\"\n"
                    "export ANTO426_BASE_ALT=\"%s\"\n"
                    "export ANTO426_TITLEBAR=\"%s\"\n"
                    "export ANTO426_TITLEBAR_BACKDROP=\"%s\"\n"
                    "export ANTO426_POPOVER=\"%s\"\n"
                    "export ANTO426_FOREGROUND=\"%s\"\n"
                    "export ANTO426_MUTED=\"%s\"\n"
                    "export ANTO426_SELECT=\"%s\"\n"
                    "export ANTO426_ACCENT=\"%s\"\n"
                    "export ANTO426_SELECTED_FG=\"%s\"\n"
                    "export ANTO426_BORDER=\"%s\"\n"
                    "export ANTO426_PANEL_BG=\"%s\"\n"
                    "export ANTO426_PANEL_BG_HOVER=\"%s\"\n"
                    "export ANTO426_OVERLAY_BG=\"%s\"\n"
                    "export ANTO426_ITEM_BG=\"%s\"\n"
                    "export ANTO426_ITEM_BG_HOVER=\"%s\"\n"
                    "export ANTO426_ITEM_BG_ACTIVE=\"%s\"\n"
                    "export ANTO426_ACCENT_SOFT=\"%s\"\n"
                    "export ANTO426_ACCENT_STRONG=\"%s\"\n"
                    "export ANTO426_BORDER_SOFT=\"%s\"\n"
                    "export ANTO426_BORDER_MEDIUM=\"%s\"\n"
                    "export ANTO426_SHADOW_SOFT=\"%s\"\n"
                    "export ANTO426_SHADOW_MEDIUM=\"%s\"\n"
                    "export ANTO426_WALLPAPER=\"%s\"\n",
                    p->background.hex, p->surface.hex, p->base.hex, p->base_alt.hex, p->titlebar.hex,
                    p->titlebar_backdrop.hex, p->popover.hex, p->foreground.hex, p->muted.hex, p->select.hex,
                    p->accent.hex, p->selected_fg.hex, p->border.hex, css_panel_bg, css_panel_bg_hover,
                    css_overlay_bg, css_item_bg, css_item_bg_hover, css_item_bg_active, css_accent_soft,
                    css_accent_strong, css_border_soft, css_border_medium, css_shadow_soft, css_shadow_medium,
                    current_wallpaper_path);
}

static void write_hypr_theme(const Paths *paths, const Palette *p) {
    Color panel, glass_border;
    set_color(&panel, p->background.r * 0.75 * 0.86 + p->accent.r * 0.14,
              p->background.g * 0.75 * 0.86 + p->accent.g * 0.14,
              p->background.b * 0.75 * 0.86 + p->accent.b * 0.14);
    set_color(&glass_border, p->foreground.r * 0.8 + p->accent.r * 0.2,
              p->foreground.g * 0.8 + p->accent.g * 0.2,
              p->foreground.b * 0.8 + p->accent.b * 0.2);

    write_text_file(paths->hypr_theme_file,
                    "# Generated by wallpaper_core.c\n"
                    "$anto426_wallpaper = %s/normal.png\n"
                    "$anto426_background = rgb(%s)\n"
                    "$anto426_surface = rgb(%s)\n"
                    "$anto426_foreground = rgb(%s)\n"
                    "$anto426_muted = rgb(%s)\n"
                    "$anto426_accent = rgb(%s)\n"
                    "$anto426_border = rgb(%s)\n"
                    "$anto426_muted_hex = %s\n\n"
                    "$anto426_active_border = rgba(%see)\n"
                    "$anto426_inactive_border = rgba(%saa)\n"
                    "$anto426_shadow = rgba(00000055)\n"
                    "$anto426_panel_bg = rgba(%s%02x)\n"
                    "$anto426_panel_bg_hover = rgba(%sc7)\n"
                    "$anto426_overlay_bg = rgba(%s47)\n"
                    "$anto426_item_bg = rgba(%s2e)\n"
                    "$anto426_item_bg_hover = rgba(%s4d)\n"
                    "$anto426_item_bg_active = rgba(%s6b)\n"
                    "$anto426_border_medium = rgba(%s57)\n"
                    "$anto426_shadow_soft = rgba(%s2e)\n"
                    "$anto426_shadow_medium = rgba(%s4d)\n"
                    "$anto426_background_panel = rgba(%scc)\n"
                    "$anto426_background_soft = rgba(%s99)\n"
                    "$anto426_surface_panel = rgba(%sc7)\n"
                    "$anto426_border_panel = rgba(%s35)\n"
                    "$anto426_foreground_strong = rgba(%se6)\n"
                    "$anto426_accent_strong = rgba(%sf2)\n"
                    "$anto426_accent_soft = rgba(%scc)\n"
                    "$anto426_surface_soft = rgba(%s11)\n"
                    "$anto426_border_soft = rgba(%s66)\n"
                    "$anto426_error = rgb(%s)\n"
                    "$anto426_warning = rgb(%s)\n"
                    "$anto426_success = rgb(%s)\n",
                    paths->cache_awww,
                    hex_no_hash(&p->background), hex_no_hash(&p->surface), hex_no_hash(&p->foreground),
                    hex_no_hash(&p->muted), hex_no_hash(&p->accent), hex_no_hash(&p->border), p->muted.hex,
                    hex_no_hash(&p->accent), hex_no_hash(&p->background), hex_no_hash(&panel),
                    (int)lround(ANTO_MATERIAL_OPACITY * 255), hex_no_hash(&p->background), hex_no_hash(&p->background), hex_no_hash(&p->surface),
                    hex_no_hash(&p->select), hex_no_hash(&p->select), hex_no_hash(&p->border),
                    hex_no_hash(&p->background), hex_no_hash(&p->background), hex_no_hash(&p->background),
                    hex_no_hash(&p->background), hex_no_hash(&p->surface), hex_no_hash(&glass_border),
                    hex_no_hash(&p->foreground), hex_no_hash(&p->accent), hex_no_hash(&p->accent),
                    hex_no_hash(&p->foreground), hex_no_hash(&p->border),
                    hex_no_hash(&p->red), hex_no_hash(&p->yellow), hex_no_hash(&p->green));
}

static void write_terminal_theme(const Paths *paths, const Palette *p) {
    char time_buf[64];
    time_t now;
    struct tm tm_info;

    now = time(NULL);
    localtime_r(&now, &tm_info);
    strftime(time_buf, sizeof(time_buf), "%Y-%m-%dT%H:%M:%S", &tm_info);
    write_text_file(paths->ghostty_dynamic_file,
                    "# Generated by wallpaper_core.c at %s\n"
                    "palette = 0=%s\n"
                    "palette = 1=%s\n"
                    "palette = 2=%s\n"
                    "palette = 3=%s\n"
                    "palette = 4=%s\n"
                    "palette = 5=%s\n"
                    "palette = 6=%s\n"
                    "palette = 7=%s\n"
                    "palette = 8=%s\n"
                    "palette = 9=%s\n"
                    "palette = 10=%s\n"
                    "palette = 11=%s\n"
                    "palette = 12=%s\n"
                    "palette = 13=%s\n"
                    "palette = 14=%s\n"
                    "palette = 15=ffffff\n"
                    "background = %s\n"
                    "foreground = %s\n"
                    "cursor-color = %s\n"
                    "cursor-text = %s\n"
                    "selection-background = %s\n"
                    "selection-foreground = %s\n",
                    time_buf, hex_no_hash(&p->background), hex_no_hash(&p->red), hex_no_hash(&p->green),
                    hex_no_hash(&p->yellow), hex_no_hash(&p->accent), hex_no_hash(&p->pink),
                    hex_no_hash(&p->muted), hex_no_hash(&p->foreground), hex_no_hash(&p->gray),
                    hex_no_hash(&p->red), hex_no_hash(&p->green), hex_no_hash(&p->yellow),
                    hex_no_hash(&p->accent), hex_no_hash(&p->purple), hex_no_hash(&p->muted),
                    hex_no_hash(&p->background), hex_no_hash(&p->foreground), hex_no_hash(&p->accent),
                    hex_no_hash(&p->background), hex_no_hash(&p->select), hex_no_hash(&p->foreground));
}

static void write_palette_map(const Paths *paths, const Palette *p, const char *current_wallpaper_path) {
    char map_file[PATH_MAX];
    snprintf(map_file, sizeof(map_file), "%s/palette.map", paths->state_dir);
    write_text_file(map_file,
                    "# Generated by wallpaper_core.c - palette routing map\n"
                    "# wallpaper=%s\n\n"
                    "[core]\n"
                    "background=%s\n"
                    "surface=%s\n"
                    "base=%s\n"
                    "base_alt=%s\n"
                    "foreground=%s\n"
                    "muted=%s\n"
                    "accent=%s\n"
                    "select=%s\n"
                    "border=%s\n"
                    "selected_fg=%s\n\n"
                    "[semantic]\n"
                    "titlebar=%s\n"
                    "titlebar_backdrop=%s\n"
                    "popover=%s\n"
                    "red=%s\n"
                    "orange=%s\n"
                    "yellow=%s\n"
                    "green=%s\n"
                    "pink=%s\n"
                    "purple=%s\n"
                    "gray=%s\n\n"
                    "[targets.session]\n"
                    "hypr=%s\n"
                    "colors=%s/colors.{css,rasi,sh}\n"
                    "ghostty=%s\n"
                    "vscode=%s\n"
                    "htop=%s/htoprc\n\n"
                    "[targets.apps]\n"
                    "gtk3=%s/gtk.css\n"
                    "gtk4=%s/gtk.css\n"
                    "qt5ct=%s/colors/anto426.conf\n"
                    "qt6ct=%s/colors/anto426.conf\n"
                    "kvantum=%s\n\n"
                    "[domain.boot-login]\n"
                    "state=%s\n"
                    "policy=isolated-explicit-target\n\n"
                    "[htop.tokens]\n"
                    "color_scheme=0\n"
                    "source=terminal-ansi-palette\n",
                    current_wallpaper_path, p->background.hex, p->surface.hex, p->base.hex, p->base_alt.hex,
                    p->foreground.hex, p->muted.hex, p->accent.hex, p->select.hex, p->border.hex,
                    p->selected_fg.hex, p->titlebar.hex, p->titlebar_backdrop.hex, p->popover.hex, p->red.hex,
                    p->orange.hex, p->yellow.hex, p->green.hex, p->pink.hex, p->purple.hex, p->gray.hex,
                    paths->hypr_theme_file, paths->colors_dir,
                    paths->ghostty_dynamic_file, vscode_theme_file,
                    paths->htop_config_dir, paths->gtk3_dir,
                    paths->gtk4_dir, paths->qt5ct_dir,
                    paths->qt6ct_dir, paths->kvantum_theme_dir,
                    paths->boot_login_state);
}

typedef struct {
    double l;
    double a;
    double b;
} Oklab;

static double srgb_unit_to_linear(double value) {
    return value <= 0.04045 ? value / 12.92
                            : pow((value + 0.055) / 1.055, 2.4);
}

static double linear_to_srgb_unit(double value) {
    if (value <= 0.0) return 0.0;
    if (value >= 1.0) return 1.0;
    return value <= 0.0031308 ? value * 12.92
                             : 1.055 * pow(value, 1.0 / 2.4) - 0.055;
}

static Oklab rgb_to_oklab(int r, int g, int b) {
    double lr = srgb_unit_to_linear((double)r / 255.0);
    double lg = srgb_unit_to_linear((double)g / 255.0);
    double lb = srgb_unit_to_linear((double)b / 255.0);
    double l = 0.4122214708 * lr + 0.5363325363 * lg + 0.0514459929 * lb;
    double m = 0.2119034982 * lr + 0.6806995451 * lg + 0.1073969566 * lb;
    double s = 0.0883024619 * lr + 0.2817188376 * lg + 0.6299787005 * lb;
    double l_root = cbrt(l);
    double m_root = cbrt(m);
    double s_root = cbrt(s);
    Oklab result;

    result.l = 0.2104542553 * l_root + 0.7936177850 * m_root -
               0.0040720468 * s_root;
    result.a = 1.9779984951 * l_root - 2.4285922050 * m_root +
               0.4505937099 * s_root;
    result.b = 0.0259040371 * l_root + 0.7827717662 * m_root -
               0.8086757660 * s_root;
    return result;
}

static void oklab_to_rgb(Oklab color, int *r, int *g, int *b) {
    double l_root = color.l + 0.3963377774 * color.a +
                    0.2158037573 * color.b;
    double m_root = color.l - 0.1055613458 * color.a -
                    0.0638541728 * color.b;
    double s_root = color.l - 0.0894841775 * color.a -
                    1.2914855480 * color.b;
    double l = l_root * l_root * l_root;
    double m = m_root * m_root * m_root;
    double s = s_root * s_root * s_root;
    double lr = 4.0767416621 * l - 3.3077115913 * m + 0.2309699292 * s;
    double lg = -1.2684380046 * l + 2.6097574011 * m - 0.3413193965 * s;
    double lb = -0.0041960863 * l - 0.7034186147 * m + 1.7076147010 * s;

    *r = clamp_channel(linear_to_srgb_unit(lr) * 255.0);
    *g = clamp_channel(linear_to_srgb_unit(lg) * 255.0);
    *b = clamp_channel(linear_to_srgb_unit(lb) * 255.0);
}

static double oklab_distance_squared(Oklab left, Oklab right) {
    double dl = left.l - right.l;
    double da = left.a - right.a;
    double db = left.b - right.b;
    return dl * dl + da * da + db * db;
}

static int compare_assignment_output(const void *left, const void *right) {
    const WallpaperAssignment *a = left;
    const WallpaperAssignment *b = right;
    int output_cmp = strcmp(a->output, b->output);
    return output_cmp != 0 ? output_cmp : strcmp(a->asset, b->asset);
}

static int compare_asset_path(const void *left, const void *right) {
    const WallpaperAssetSample *a = left;
    const WallpaperAssetSample *b = right;
    return strcmp(a->asset, b->asset);
}

static int parse_rgb_hex(const char *text, int *r, int *g, int *b) {
    const char *hex = text;
    int values[6];

    if (!hex) return 0;
    if (*hex == '#') hex++;
    if (strlen(hex) != 6) return 0;
    for (int index = 0; index < 6; index++) {
        values[index] = hex_digit_value(hex[index]);
        if (values[index] < 0) return 0;
    }
    *r = values[0] * 16 + values[1];
    *g = values[2] * 16 + values[3];
    *b = values[4] * 16 + values[5];
    return 1;
}

static int aggregate_wallpaper_samples(WallpaperPaletteAggregate *aggregate) {
    double total_weight = 0.0;
    Oklab average = {0.0, 0.0, 0.0};
    double best_score = HUGE_VAL;
    size_t best_accent = 0;

    aggregate->asset_count = 0;
    aggregate->dominant_asset = 0;
    if (aggregate->assignment_count == 0) return 0;

    qsort(aggregate->assignments, aggregate->assignment_count,
          sizeof(aggregate->assignments[0]), compare_assignment_output);

    for (size_t index = 0; index < aggregate->assignment_count; index++) {
        WallpaperAssignment *assignment = &aggregate->assignments[index];
        WallpaperAssetSample *asset = NULL;

        for (size_t candidate = 0; candidate < aggregate->asset_count;
             candidate++) {
            if (strcmp(aggregate->assets[candidate].asset,
                       assignment->asset) == 0) {
                asset = &aggregate->assets[candidate];
                break;
            }
        }
        if (!asset) {
            if (aggregate->asset_count >= MAX_WALLPAPER_ASSETS) return 0;
            asset = &aggregate->assets[aggregate->asset_count++];
            memset(asset, 0, sizeof(*asset));
            copy_string(asset->asset, sizeof(asset->asset), assignment->asset);
            copy_string(asset->kind, sizeof(asset->kind), assignment->kind);
            asset->avg_r = assignment->avg_r;
            asset->avg_g = assignment->avg_g;
            asset->avg_b = assignment->avg_b;
            asset->accent_r = assignment->accent_r;
            asset->accent_g = assignment->accent_g;
            asset->accent_b = assignment->accent_b;
            copy_string(asset->sample_path, sizeof(asset->sample_path),
                        assignment->sample_path);
        }
        asset->weight += assignment->logical_area > 0.0
                             ? assignment->logical_area
                             : 1.0;
    }

    qsort(aggregate->assets, aggregate->asset_count,
          sizeof(aggregate->assets[0]), compare_asset_path);
    for (size_t index = 0; index < aggregate->asset_count; index++) {
        WallpaperAssetSample *asset = &aggregate->assets[index];
        Oklab sample = rgb_to_oklab(asset->avg_r, asset->avg_g,
                                    asset->avg_b);
        average.l += sample.l * asset->weight;
        average.a += sample.a * asset->weight;
        average.b += sample.b * asset->weight;
        total_weight += asset->weight;
        if (asset->weight > aggregate->assets[aggregate->dominant_asset].weight) {
            aggregate->dominant_asset = index;
        }
    }
    if (total_weight <= 0.0) return 0;
    average.l /= total_weight;
    average.a /= total_weight;
    average.b /= total_weight;
    oklab_to_rgb(average, &aggregate->average_r, &aggregate->average_g,
                 &aggregate->average_b);

    /*
     * Choose a vivid weighted medoid instead of averaging accent RGB values.
     * It is representative of the complete set, but stays on a real
     * wallpaper hue and therefore never collapses complementary colors into
     * grey/brown.  Sorting by canonical asset makes exact ties deterministic.
     */
    for (size_t index = 0; index < aggregate->asset_count; index++) {
        WallpaperAssetSample *candidate = &aggregate->assets[index];
        Oklab candidate_lab = rgb_to_oklab(candidate->accent_r,
                                           candidate->accent_g,
                                           candidate->accent_b);
        double cost = 0.0;
        double chroma = hypot(candidate_lab.a, candidate_lab.b);
        double share = candidate->weight / total_weight;
        double score;

        for (size_t other = 0; other < aggregate->asset_count; other++) {
            WallpaperAssetSample *sample = &aggregate->assets[other];
            Oklab sample_lab = rgb_to_oklab(sample->accent_r,
                                            sample->accent_g,
                                            sample->accent_b);
            cost += sample->weight *
                    oklab_distance_squared(candidate_lab, sample_lab);
        }
        score = cost / total_weight - chroma * 0.12 - share * 0.025;
        if (score < best_score - 1e-12) {
            best_score = score;
            best_accent = index;
        }
    }

    {
        WallpaperAssetSample *chosen = &aggregate->assets[best_accent];
        Oklab accent = rgb_to_oklab(chosen->accent_r, chosen->accent_g,
                                    chosen->accent_b);
        /* A small perceptual chroma lift keeps the global anchor lively. */
        accent.a *= 1.08;
        accent.b *= 1.08;
        oklab_to_rgb(accent, &aggregate->accent_r, &aggregate->accent_g,
                     &aggregate->accent_b);
    }
    return 1;
}

static void write_htop_theme(const Paths *paths, const Paths *log_paths);
static void write_btop_theme(const Paths *paths, const Palette *p, const Paths *log_paths);

typedef enum {
    CORE_WRITE_COLORS,
    CORE_WRITE_HYPR,
    CORE_WRITE_TERMINAL,
    CORE_WRITE_HTOP,
    CORE_WRITE_BTOP,
    CORE_WRITE_PALETTE_MAP
} CoreWriteTask;

static void run_core_write_task(CoreWriteTask task, const Paths *paths, const Palette *p, const char *current_wallpaper_path) {
    switch (task) {
        case CORE_WRITE_COLORS:
            write_color_files(paths, p, current_wallpaper_path);
            break;
        case CORE_WRITE_HYPR:
            write_hypr_theme(paths, p);
            break;
        case CORE_WRITE_TERMINAL:
            write_terminal_theme(paths, p);
            break;
        case CORE_WRITE_HTOP:
            write_htop_theme(paths, paths);
            break;
        case CORE_WRITE_BTOP:
            write_btop_theme(paths, p, paths);
            break;
        case CORE_WRITE_PALETTE_MAP:
            write_palette_map(paths, p, current_wallpaper_path);
            break;
    }
}

static void write_core_theme_parallel(const Paths *paths, const Palette *p, const char *current_wallpaper_path) {
    const CoreWriteTask tasks[] = {
        CORE_WRITE_COLORS,
        CORE_WRITE_HYPR,
        CORE_WRITE_TERMINAL,
        CORE_WRITE_HTOP,
        CORE_WRITE_BTOP,
        CORE_WRITE_PALETTE_MAP
    };
    pid_t pids[sizeof(tasks) / sizeof(tasks[0])];
    size_t pid_count = 0;

    for (size_t i = 0; i < sizeof(tasks) / sizeof(tasks[0]); i++) {
        pid_t pid = fork();
        if (pid == 0) {
            run_core_write_task(tasks[i], paths, p, current_wallpaper_path);
            _exit(0);
        }
        if (pid > 0) {
            pids[pid_count++] = pid;
        } else {
            run_core_write_task(tasks[i], paths, p, current_wallpaper_path);
        }
    }

    for (size_t i = 0; i < pid_count; i++) {
        int status = 0;
        if (waitpid(pids[i], &status, 0) < 0 || !WIFEXITED(status) || WEXITSTATUS(status) != 0) {
            append_log(paths, "Core theme writer failed: pid=%ld", (long)pids[i]);
        }
    }
}

static void write_htop_theme(const Paths *paths, const Paths *log_paths) {
    char target[PATH_MAX];
    char tmp[PATH_MAX];
    FILE *in;
    FILE *out;
    char version[128] = "3.5.1";
    char cmd_out[256];
    int seen_color = 0, seen_base = 0, seen_megabytes = 0;

    snprintf(target, sizeof(target), "%s/htoprc", paths->htop_config_dir);
    if (regular_file_exists(target)) {
        snprintf(tmp, sizeof(tmp), "%s.tmp.%ld", target, (long)getpid());
        in = fopen(target, "r");
        out = fopen(tmp, "w");
        if (!in || !out) {
            if (in) fclose(in);
            if (out) fclose(out);
            return;
        }
        char *line = NULL;
        size_t line_cap = 0;
        while (getline(&line, &line_cap, in) != -1) {
            if (starts_with(line, "color_scheme=")) {
                fputs("color_scheme=0\n", out);
                seen_color = 1;
            } else if (starts_with(line, "highlight_base_name=")) {
                fputs("highlight_base_name=1\n", out);
                seen_base = 1;
            } else if (starts_with(line, "highlight_megabytes=")) {
                fputs("highlight_megabytes=1\n", out);
                seen_megabytes = 1;
            } else {
                fputs(line, out);
            }
        }
        free(line);
        if (!seen_color) fputs("color_scheme=0\n", out);
        if (!seen_base) fputs("highlight_base_name=1\n", out);
        if (!seen_megabytes) fputs("highlight_megabytes=1\n", out);
        fclose(in);
        fclose(out);
        rename(tmp, target);
        append_log(log_paths, "htop palette aggiornata: %s", target);
        return;
    }

    if (capture_command("htop --version 2>/dev/null | awk 'NR == 1 { print $2; exit }'", cmd_out, sizeof(cmd_out))) {
        copy_string(version, sizeof(version), cmd_out);
    }
    write_text_file(target,
                    "# Generated by wallpaper_core.c\n"
                    "htop_version=%s\n"
                    "config_reader_min_version=3\n"
                    "fields=0 48 17 18 38 39 130 2 46 47 49 1\n"
                    "hide_kernel_threads=1\n"
                    "hide_userland_threads=0\n"
                    "hide_running_in_container=0\n"
                    "shadow_other_users=1\n"
                    "show_thread_names=0\n"
                    "show_program_path=1\n"
                    "highlight_base_name=1\n"
                    "highlight_deleted_exe=1\n"
                    "shadow_distribution_path_prefix=0\n"
                    "highlight_megabytes=1\n"
                    "highlight_threads=1\n"
                    "highlight_changes=0\n"
                    "highlight_changes_delay_secs=5\n"
                    "find_comm_in_cmdline=1\n"
                    "strip_exe_from_cmdline=1\n"
                    "show_merged_command=0\n"
                    "header_margin=1\n"
                    "screen_tabs=1\n"
                    "detailed_cpu_time=0\n"
                    "cpu_count_from_one=1\n"
                    "show_cpu_usage=1\n"
                    "show_cpu_frequency=1\n"
                    "show_cpu_temperature=1\n"
                    "degree_fahrenheit=0\n"
                    "update_process_names=0\n"
                    "account_guest_in_cpu_meter=0\n"
                    "color_scheme=0\n"
                    "enable_mouse=1\n"
                    "delay=15\n"
                    "hide_function_bar=0\n"
                    "header_layout=two_50_50\n"
                    "column_meters_0=LeftCPUs Memory Swap\n"
                    "column_meter_modes_0=1 1 1\n"
                    "column_meters_1=RightCPUs Tasks LoadAverage Uptime\n"
                    "column_meter_modes_1=1 2 2 2\n"
                    "tree_view=0\n"
                    "sort_key=46\n"
                    "tree_sort_key=0\n"
                    "sort_direction=-1\n"
                    "tree_sort_direction=1\n"
                    "tree_view_always_by_pid=0\n"
                    "all_branches_collapsed=0\n"
                    "screen:Main=PID USER PRIORITY NICE M_VIRT M_RESIDENT M_PRIV STATE PERCENT_CPU PERCENT_MEM TIME Command\n"
                    ".sort_key=PERCENT_CPU\n"
                    ".tree_sort_key=PID\n"
                    ".tree_view=0\n"
                    ".tree_view_always_by_pid=0\n"
                    ".sort_direction=-1\n"
                    ".tree_sort_direction=1\n"
                    ".all_branches_collapsed=0\n"
                    "screen:I/O=PID USER IO_PRIORITY IO_RATE IO_READ_RATE IO_WRITE_RATE PERCENT_SWAP_DELAY PERCENT_IO_DELAY Command\n"
                    ".sort_key=IO_RATE\n"
                    ".tree_sort_key=PID\n"
                    ".tree_view=0\n"
                    ".tree_view_always_by_pid=0\n"
                    ".sort_direction=-1\n"
                    ".tree_sort_direction=1\n"
                    ".all_branches_collapsed=0\n",
                    version);
    append_log(log_paths, "htop palette creata: %s", target);
}

static void write_btop_theme(const Paths *paths, const Palette *p, const Paths *log_paths) {
    char config[PATH_MAX];
    char theme[PATH_MAX];
    char tmp[PATH_MAX];
    FILE *in;
    FILE *out;
    int seen_theme = 0, seen_background = 0, seen_truecolor = 0;

    snprintf(config, sizeof(config), "%s/btop.conf", paths->btop_config_dir);
    snprintf(theme, sizeof(theme), "%s/anto426.theme", paths->btop_theme_dir);
    write_text_file(theme,
                    "# Generated by wallpaper_core.c\n"
                    "theme[main_bg]=\"%s\"\n"
                    "theme[main_fg]=\"%s\"\n"
                    "theme[title]=\"%s\"\n"
                    "theme[hi_fg]=\"%s\"\n"
                    "theme[selected_bg]=\"%s\"\n"
                    "theme[selected_fg]=\"%s\"\n"
                    "theme[inactive_fg]=\"%s\"\n"
                    "theme[proc_misc]=\"%s\"\n"
                    "theme[cpu_box]=\"%s\"\n"
                    "theme[mem_box]=\"%s\"\n"
                    "theme[net_box]=\"%s\"\n"
                    "theme[proc_box]=\"%s\"\n"
                    "theme[div_line]=\"%s\"\n"
                    "theme[temp_start]=\"%s\"\n"
                    "theme[temp_mid]=\"%s\"\n"
                    "theme[temp_end]=\"%s\"\n"
                    "theme[cpu_start]=\"%s\"\n"
                    "theme[cpu_mid]=\"%s\"\n"
                    "theme[cpu_end]=\"%s\"\n"
                    "theme[free_start]=\"%s\"\n"
                    "theme[free_mid]=\"%s\"\n"
                    "theme[free_end]=\"%s\"\n"
                    "theme[cached_start]=\"%s\"\n"
                    "theme[cached_mid]=\"%s\"\n"
                    "theme[cached_end]=\"%s\"\n"
                    "theme[available_start]=\"%s\"\n"
                    "theme[available_mid]=\"%s\"\n"
                    "theme[available_end]=\"%s\"\n"
                    "theme[used_start]=\"%s\"\n"
                    "theme[used_mid]=\"%s\"\n"
                    "theme[used_end]=\"%s\"\n"
                    "theme[download_start]=\"%s\"\n"
                    "theme[download_mid]=\"%s\"\n"
                    "theme[download_end]=\"%s\"\n"
                    "theme[upload_start]=\"%s\"\n"
                    "theme[upload_mid]=\"%s\"\n"
                    "theme[upload_end]=\"%s\"\n",
                    p->background.hex, p->foreground.hex, p->foreground.hex, p->accent.hex, p->select.hex,
                    p->selected_fg.hex, p->muted.hex, p->accent.hex, p->border.hex, p->border.hex,
                    p->border.hex, p->border.hex, p->border.hex, p->green.hex, p->yellow.hex, p->red.hex,
                    p->green.hex, p->yellow.hex, p->red.hex, p->green.hex, p->yellow.hex, p->red.hex,
                    p->muted.hex, p->accent.hex, p->purple.hex, p->green.hex, p->accent.hex, p->purple.hex,
                    p->green.hex, p->yellow.hex, p->red.hex, p->green.hex, p->accent.hex, p->purple.hex,
                    p->pink.hex, p->orange.hex, p->red.hex);

    if (regular_file_exists(config)) {
        snprintf(tmp, sizeof(tmp), "%s.tmp.%ld", config, (long)getpid());
        in = fopen(config, "r");
        out = fopen(tmp, "w");
        if (in && out) {
            char *line = NULL;
            size_t line_cap = 0;
            while (getline(&line, &line_cap, in) != -1) {
                char *p_line = line;
                while (*p_line && isspace((unsigned char)*p_line)) p_line++;
                if (starts_with(p_line, "color_theme")) {
                    fputs("color_theme = \"anto426\"\n", out);
                    seen_theme = 1;
                } else if (starts_with(p_line, "theme_background")) {
                    fputs("theme_background = false\n", out);
                    seen_background = 1;
                } else if (starts_with(p_line, "truecolor")) {
                    fputs("truecolor = true\n", out);
                    seen_truecolor = 1;
                } else {
                    fputs(line, out);
                }
            }
            free(line);
            if (!seen_theme) fputs("color_theme = \"anto426\"\n", out);
            if (!seen_background) fputs("theme_background = false\n", out);
            if (!seen_truecolor) fputs("truecolor = true\n", out);
            fclose(in);
            fclose(out);
            rename(tmp, config);
        } else {
            if (in) fclose(in);
            if (out) fclose(out);
        }
    } else {
        write_text_file(config,
                        "# Generated by wallpaper_core.c\n"
                        "color_theme = \"anto426\"\n"
                        "theme_background = false\n"
                        "truecolor = true\n"
                        "rounded_corners = true\n"
                        "graph_symbol = \"braille\"\n"
                        "shown_boxes = \"cpu mem net proc\"\n"
                        "update_ms = 2000\n"
                        "proc_sorting = \"cpu lazy\"\n"
                        "proc_colors = true\n"
                        "proc_gradient = true\n"
                        "proc_cpu_graphs = true\n"
                        "check_temp = true\n"
                        "show_coretemp = true\n"
                        "show_cpu_freq = true\n"
                        "show_uptime = true\n"
                        "mem_graphs = true\n"
                        "show_disks = true\n"
                        "net_auto = true\n");
    }
    append_log(log_paths, "btop palette aggiornata: %s", theme);
}

static void fprint_shell_var(FILE *f, const char *name, const char *value) {
    char *q = shell_quote(value ? value : "");
    if (!q) {
        return;
    }
    fprintf(f, "%s=%s\n", name, q);
    free(q);
}

static void write_desktop_palette_state(
    const Paths *paths, const WallpaperPaletteAggregate *aggregate,
    const Palette *palette) {
    char *buffer = NULL;
    size_t size = 0;
    FILE *stream = open_memstream(&buffer, &size);
    Color sampled_average;
    Color sampled_accent;

    if (!stream) return;
    set_color(&sampled_average, aggregate->average_r, aggregate->average_g,
              aggregate->average_b);
    set_color(&sampled_accent, aggregate->accent_r, aggregate->accent_g,
              aggregate->accent_b);
    fputs("# Generated desktop wallpaper palette state\n", stream);
    fputs("domain=desktop\n", stream);
    fputs("algorithm=oklab-weighted-mean+vivid-weighted-medoid-v1\n",
          stream);
    fprintf(stream, "output_count=%zu\nasset_count=%zu\n",
            aggregate->assignment_count, aggregate->asset_count);
    fprint_shell_var(stream, "sample_average", sampled_average.hex);
    fprint_shell_var(stream, "sample_accent", sampled_accent.hex);
    fprint_shell_var(stream, "background", palette->background.hex);
    fprint_shell_var(stream, "accent", palette->accent.hex);
    for (size_t index = 0; index < aggregate->asset_count; index++) {
        char name[64];
        snprintf(name, sizeof(name), "asset_%zu", index);
        fprint_shell_var(stream, name, aggregate->assets[index].asset);
        fprintf(stream, "asset_%zu_weight=%.6f\n", index,
                aggregate->assets[index].weight);
    }
    if (fclose(stream) == 0 && buffer) {
        write_text_file(paths->desktop_palette_state, "%s", buffer);
    }
    free(buffer);
}

static void write_module_env(const Paths *paths, const Palette *p, const Canvas *canvas,
                             const char *source_wallpaper_path, const char *current_wallpaper_path,
                             char *env_path, size_t env_path_size) {
    FILE *f;

    snprintf(env_path, env_path_size, "%s/desktop-theme-XXXXXX", paths->state_dir);
    ensure_parent_dir(env_path);
    int descriptor = mkstemp(env_path);
    f = descriptor >= 0 ? fdopen(descriptor, "w") : NULL;
    if (!f) {
        if (descriptor >= 0) { close(descriptor); unlink(env_path); }
        env_path[0] = '\0';
        return;
    }

    fprintf(f, "# Immutable palette snapshot for one background job\n");
    fprint_shell_var(f, "effects_snapshot", "1");
    fprint_shell_var(f, "background", p->background.hex);
    fprint_shell_var(f, "surface", p->surface.hex);
    fprint_shell_var(f, "select", p->select.hex);
    fprint_shell_var(f, "accent", p->accent.hex);
    fprint_shell_var(f, "border", p->border.hex);
    fprint_shell_var(f, "foreground", p->foreground.hex);
    fprint_shell_var(f, "muted", p->muted.hex);
    fprint_shell_var(f, "red", p->red.hex);
    fprint_shell_var(f, "orange", p->orange.hex);
    fprint_shell_var(f, "yellow", p->yellow.hex);
    fprint_shell_var(f, "green", p->green.hex);
    fprint_shell_var(f, "pink", p->pink.hex);
    fprint_shell_var(f, "purple", p->purple.hex);
    fprint_shell_var(f, "gray", p->gray.hex);
    fprint_shell_var(f, "base", p->base.hex);
    fprint_shell_var(f, "base_alt", p->base_alt.hex);
    fprint_shell_var(f, "titlebar", p->titlebar.hex);
    fprint_shell_var(f, "titlebar_backdrop", p->titlebar_backdrop.hex);
    fprint_shell_var(f, "popover", p->popover.hex);
    fprint_shell_var(f, "selected_fg", p->selected_fg.hex);
    fprint_shell_var(f, "panel_bg", p->panel_bg);
    fprint_shell_var(f, "panel_bg_hover", p->panel_bg_hover);
    fprint_shell_var(f, "overlay_bg", p->overlay_bg);
    fprint_shell_var(f, "item_bg", p->item_bg);
    fprint_shell_var(f, "item_bg_hover", p->item_bg_hover);
    fprint_shell_var(f, "item_bg_active", p->item_bg_active);
    fprint_shell_var(f, "accent_soft", p->accent_soft);
    fprint_shell_var(f, "accent_strong", p->accent_strong);
    fprint_shell_var(f, "border_soft", p->border_soft);
    fprint_shell_var(f, "border_medium", p->border_medium);
    fprint_shell_var(f, "shadow_soft", p->shadow_soft);
    fprint_shell_var(f, "shadow_medium", p->shadow_medium);
    fprint_shell_var(f, "background_alpha", p->background_alpha);
    fprint_shell_var(f, "surface_alpha", p->surface_alpha);
    fprint_shell_var(f, "select_alpha", p->select_alpha);
    fprint_shell_var(f, "accent_alpha", p->accent_alpha);
    fprint_shell_var(f, "border_alpha", p->border_alpha);

    fprint_shell_var(f, "source_wallpaper_path", source_wallpaper_path);
    fprint_shell_var(f, "current_wallpaper_path", current_wallpaper_path);
    fprint_shell_var(f, "effects_expected", source_wallpaper_path);
    fprint_shell_var(f, "destination_wallpaper_dir", paths->cache_awww);
    fprint_shell_var(f, "colors_dir", paths->colors_dir);
    fprint_shell_var(f, "hypr_theme_file", paths->hypr_theme_file);
    fprint_shell_var(f, "ghostty_theme_dir", paths->ghostty_theme_dir);
    fprint_shell_var(f, "ghostty_dynamic_file",
                     paths->ghostty_dynamic_file);
    fprint_shell_var(f, "htop_config_dir", paths->htop_config_dir);
    fprint_shell_var(f, "btop_config_dir", paths->btop_config_dir);
    fprint_shell_var(f, "btop_theme_dir", paths->btop_theme_dir);
    fprint_shell_var(f, "gtk3_dir", paths->gtk3_dir);
    fprint_shell_var(f, "gtk4_dir", paths->gtk4_dir);
    fprint_shell_var(f, "kvantum_dir", paths->kvantum_dir);
    fprint_shell_var(f, "kvantum_theme_dir", paths->kvantum_theme_dir);
    fprint_shell_var(f, "kvantum_config_file", paths->kvantum_config_file);
    fprint_shell_var(f, "qt5ct_dir", paths->qt5ct_dir);
    fprint_shell_var(f, "qt6ct_dir", paths->qt6ct_dir);
    fprint_shell_var(f, "state_dir", paths->state_dir);
    fprint_shell_var(f, "log_file", paths->log_file);
    fprint_shell_var(f, "widgets_script", paths->widgets_script);
    fprint_shell_var(f, "grub_theme_dir", paths->grub_theme_dir);
    fprint_shell_var(f, "grub_background", paths->grub_background);
    fprint_shell_var(f, "grub_theme", paths->grub_theme);
    fprint_shell_var(f, "grub_select_c", paths->grub_select_c);
    fprint_shell_var(f, "grub_select_e", paths->grub_select_e);
    fprint_shell_var(f, "grub_select_w", paths->grub_select_w);
    fprint_shell_var(f, "sddm_background", paths->sddm_background);
    fprint_shell_var(f, "sddm_theme", paths->sddm_theme);
    fprint_shell_var(f, "vscode_theme_name", vscode_theme_name);
    fprint_shell_var(f, "vscode_theme_file", vscode_theme_file);
    fprint_shell_var(f, "canvas_size", canvas->text);
    fprintf(f, "canvas_width=%d\n", canvas->width);
    fprintf(f, "canvas_height=%d\n", canvas->height);
    fclose(f);
}

static int modules_bridge_enabled(const char *modules_env) {
    if (!modules_env || !modules_env[0]) {
        return 1;
    }
    return strcmp(modules_env, "0") != 0 &&
           strcasecmp(modules_env, "false") != 0 &&
           strcasecmp(modules_env, "no") != 0 &&
           strcasecmp(modules_env, "off") != 0;
}

static int modules_bridge_should_wait(const char *modules_env) {
    const char *sync_env = getenv("ANTO426_WALLPAPER_CORE_MODULES_SYNC");

    if (modules_env && strcasecmp(modules_env, "sync") == 0) {
        return 1;
    }
    if (!sync_env || !sync_env[0]) {
        return 0;
    }
    return strcmp(sync_env, "0") != 0 &&
           strcasecmp(sync_env, "false") != 0 &&
           strcasecmp(sync_env, "no") != 0 &&
           strcasecmp(sync_env, "off") != 0;
}

static void run_modules_bridge_sync(const Paths *paths, const char *env_path) {
    const char *module_phases = "gtk qt kvantum zen vscode obsidian boot";
    char *qscript;
    char *qenv;
    char cmd[PATH_MAX * 2 + 256];

    if (!regular_file_exists(paths->modules_script)) {
        append_log(paths, "Module bridge missing: %s", paths->modules_script);
        unlink(env_path);
        return;
    }
    qscript = shell_quote(paths->modules_script);
    qenv = shell_quote(env_path);
    if (!qscript || !qenv) {
        free(qscript);
        free(qenv);
        unlink(env_path);
        return;
    }
    snprintf(cmd, sizeof(cmd), "%s %s %s", qscript, qenv, module_phases);
    if (!run_shell(cmd)) {
        append_log(paths, "Module bridge failed: %s", paths->modules_script);
    }
    free(qscript);
    free(qenv);
    unlink(env_path);
}

static void start_modules_bridge(const Paths *paths, const char *env_path) {
    const char *modules_env = getenv("ANTO426_WALLPAPER_CORE_MODULES");
    const char *module_phases = "gtk qt kvantum zen vscode obsidian boot";
    char module_log[PATH_MAX];
    char *qscript;
    char *qenv;
    char *qlog;
    char cmd[PATH_MAX * 5 + 512];

    if (!modules_bridge_enabled(modules_env)) {
        append_log(paths, "Module bridge skipped by ANTO426_WALLPAPER_CORE_MODULES=%s", modules_env);
        unlink(env_path);
        return;
    }
    if (modules_bridge_should_wait(modules_env)) {
        append_log(paths, "Module bridge running synchronously");
        run_modules_bridge_sync(paths, env_path);
        return;
    }
    if (!regular_file_exists(paths->modules_script)) {
        append_log(paths, "Module bridge missing: %s", paths->modules_script);
        unlink(env_path);
        return;
    }

    snprintf(module_log, sizeof(module_log), "%s/wallpaper_modules.log", paths->state_dir);
    qscript = shell_quote(paths->modules_script);
    qenv = shell_quote(env_path);
    qlog = shell_quote(module_log);
    if (!qscript || !qenv || !qlog) {
        free(qscript);
        free(qenv);
        free(qlog);
        unlink(env_path);
        return;
    }

    snprintf(cmd, sizeof(cmd),
             "( if command -v ionice >/dev/null 2>&1; then "
             "ionice -c3 nice -n 10 %s %s %s; "
             "else nice -n 10 %s %s %s; fi ) >>%s 2>&1 &",
             qscript, qenv, module_phases, qscript, qenv, module_phases, qlog);
    if (run_shell(cmd)) {
        append_log(paths, "Module bridge started in background: %s", module_log);
    } else {
        append_log(paths, "Module bridge background start failed, running synchronously");
        run_modules_bridge_sync(paths, env_path);
    }
    free(qscript);
    free(qenv);
    free(qlog);
}

static int env_is_on(const char *name, int default_value) {
    const char *value = getenv(name);
    if (!value || !value[0]) {
        return default_value;
    }
    return strcmp(value, "0") != 0 &&
           strcasecmp(value, "false") != 0 &&
           strcasecmp(value, "no") != 0 &&
           strcasecmp(value, "off") != 0;
}

static void get_icon_paths(const Paths *paths, char *theme_name, size_t theme_name_size,
                           char *theme_dir, size_t theme_dir_size,
                           char *repo, size_t repo_size,
                           char *style, size_t style_size,
                           char *variant, size_t variant_size) {
    const char *xdg_data_home = getenv("XDG_DATA_HOME");
    const char *env_theme = getenv("ANTO426_ICON_THEME");
    const char *env_repo = getenv("ANTO426_ICON_THEME_REPO");
    const char *env_style = getenv("ANTO426_MATERIAL_SYMBOL_STYLE");
    const char *env_variant = getenv("ANTO426_MATERIAL_SYMBOL_VARIANT");

    copy_string(theme_name, theme_name_size, env_theme && env_theme[0] ? env_theme : "Anto426-Material");
    if (xdg_data_home && xdg_data_home[0]) {
        snprintf(theme_dir, theme_dir_size, "%s/icons/%s", xdg_data_home, theme_name);
    } else {
        snprintf(theme_dir, theme_dir_size, "%s/.local/share/icons/%s", paths->home, theme_name);
    }
    snprintf(repo, repo_size, "%s", env_repo && env_repo[0] ? env_repo : "");
    if (!repo[0]) {
        snprintf(repo, repo_size, "%s/Git/arch/Anto426-material-icons", paths->home);
    }
    copy_string(style, style_size, env_style && env_style[0] ? env_style : "rounded");
    for (char *p = style; *p; p++) {
        *p = (char)tolower((unsigned char)*p);
    }
    copy_string(variant, variant_size, env_variant && env_variant[0] ? env_variant : "regular");
    for (char *p = variant; *p; p++) {
        *p = (char)tolower((unsigned char)*p);
    }
}

static void icon_signature(const char *theme_name, const char *theme_dir, const char *repo,
                           const char *style, const char *variant,
                           const char *foreground, const char *surface, const char *accent,
                           const char *select, const char *border, const char *selected_fg,
                           char *out, size_t out_size) {
    char data[PATH_MAX * 3 + 512];
    unsigned long long hash;

    snprintf(data, sizeof(data),
             "theme=%s\n"
             "dir=%s\n"
             "repo=%s\n"
             "style=%s\n"
             "variant=%s\n"
             "foreground=%s\n"
             "surface=%s\n"
             "accent=%s\n"
             "select=%s\n"
             "border=%s\n"
             "selected_fg=%s\n",
             theme_name, theme_dir, repo, style, variant, foreground, surface, accent, select, border, selected_fg);
    hash = fnv1a_hash(data);
    snprintf(out, out_size, "%016llx", hash);
}

static int material_symbol_source_svg(const char *repo, const char *preferred_style, const char *variant,
                                      const char *symbol, char *out, size_t out_size) {
    const char *styles[] = {preferred_style, "rounded", "outlined", "sharp"};
    const char *suffix = strcmp(variant, "fill") == 0 ? "-fill" : "";

    for (size_t i = 0; i < sizeof(styles) / sizeof(styles[0]); i++) {
        char candidate[PATH_MAX];
        if (!styles[i] || !styles[i][0]) {
            continue;
        }
        snprintf(candidate, sizeof(candidate), "%s/vendor/source/%s/%s%s.svg", repo, styles[i], symbol, suffix);
        if (regular_file_exists(candidate)) {
            copy_string(out, out_size, candidate);
            return 1;
        }
    }
    return 0;
}

static int svg_tag_is_path(const char *tag) {
    return starts_with(tag, "<path") &&
           (tag[5] == '\0' || tag[5] == '>' || tag[5] == '/' || isspace((unsigned char)tag[5]));
}

static const char *skip_quoted_attr_value(const char *p, const char *end) {
    char quote;
    while (p < end && isspace((unsigned char)*p)) {
        p++;
    }
    if (p >= end) {
        return p;
    }
    if (*p == '"' || *p == '\'') {
        quote = *p++;
        while (p < end && *p != quote) {
            p++;
        }
        return p < end ? p + 1 : p;
    }
    while (p < end && !isspace((unsigned char)*p) && *p != '>') {
        p++;
    }
    return p;
}

static void write_path_tag_with_fill(FILE *out, const char *start, const char *end, const char *fill) {
    const char *p = start + 5;
    fprintf(out, "    <path fill=\"%s\"", fill);
    while (p < end) {
        const char *chunk = p;
        while (p < end && isspace((unsigned char)*p)) {
            p++;
        }
        if (p > chunk) {
            fputc(' ', out);
        }
        if ((end - p) >= 4 && strncasecmp(p, "fill", 4) == 0 &&
            (p + 4 == end || isspace((unsigned char)p[4]) || p[4] == '=')) {
            const char *q = p + 4;
            while (q < end && isspace((unsigned char)*q)) {
                q++;
            }
            if (q < end && *q == '=') {
                q++;
                p = skip_quoted_attr_value(q, end);
                continue;
            }
        }
        while (p < end && !isspace((unsigned char)*p)) {
            fputc(*p++, out);
        }
    }
    fputc('\n', out);
}

static int render_material_symbol_svg_c(const Paths *paths, const char *repo, const char *style, const char *variant,
                                        const char *symbol, const char *output, const char *fill) {
    char source[PATH_MAX];
    char *svg;
    FILE *out;
    const char *p;
    int found = 0;

    if (!material_symbol_source_svg(repo, style, variant, symbol, source, sizeof(source))) {
        if (strcmp(symbol, "apps") == 0 ||
            !material_symbol_source_svg(repo, style, variant, "apps", source, sizeof(source))) {
            append_log(paths, "Material Symbols SVG non disponibile nella repo: %s", symbol);
            return 0;
        }
    }

    svg = read_file_alloc(source, NULL);
    if (!svg) {
        append_log(paths, "Material Symbols SVG non leggibile: %s", source);
        return 0;
    }

    ensure_parent_dir(output);
    out = fopen(output, "w");
    if (!out) {
        free(svg);
        return 0;
    }
    fputs("<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"128\" height=\"128\" viewBox=\"0 -960 960 960\">\n", out);
    fputs("  <g transform=\"translate(96 -96) scale(0.8)\">\n", out);

    p = svg;
    while ((p = strstr(p, "<path")) != NULL) {
        const char *end = strchr(p, '>');
        if (!end) {
            break;
        }
        if (svg_tag_is_path(p)) {
            write_path_tag_with_fill(out, p, end + 1, fill);
            found = 1;
        }
        p = end + 1;
    }

    fputs("  </g>\n</svg>\n", out);
    fclose(out);
    free(svg);

    if (!found) {
        unlink(output);
        append_log(paths, "Material Symbols SVG senza path: %s", source);
    }
    return found;
}

static void write_wlogout_material_icons_c(const Paths *paths, const char *repo, const char *style, const char *variant,
                                           const char *foreground, const char *selected_fg) {
    static const struct {
        const char *name;
        const char *symbol;
    } icons[] = {
        {"lock", "lock"},
        {"suspend", "bedtime"},
        {"logout", "logout"},
        {"reboot", "restart_alt"},
        {"shutdown", "power_settings_new"},
        {"cancel", "close"}
    };
    char dir[PATH_MAX];

    snprintf(dir, sizeof(dir), "%s/.config/wlogout/icons", paths->home);
    mkdir_p(dir);
    for (size_t i = 0; i < sizeof(icons) / sizeof(icons[0]); i++) {
        char output[PATH_MAX];
        snprintf(output, sizeof(output), "%s/%s.svg", dir, icons[i].name);
        render_material_symbol_svg_c(paths, repo, style, variant, icons[i].symbol, output, foreground);
        snprintf(output, sizeof(output), "%s/%s-hover.svg", dir, icons[i].name);
        render_material_symbol_svg_c(paths, repo, style, variant, icons[i].symbol, output, selected_fg);
    }
}

static int sync_icon_theme_from_repo_c(const Paths *paths, const char *theme_name, const char *theme_dir,
                                       const char *repo, const char *style, const char *variant,
                                       const char *foreground, const char *surface, const char *accent,
                                       const char *select, const char *border) {
    char build_template[PATH_MAX];
    char *build_root;
    char source_theme[PATH_MAX];
    char build_script[PATH_MAX];
    char cmd[PATH_MAX * 8 + 1024];
    char *qrepo = NULL, *qtheme = NULL, *qbuild = NULL, *qstyle = NULL, *qvariant = NULL;
    char *qfg = NULL, *qsurface = NULL, *qaccent = NULL, *qselect = NULL, *qborder = NULL;
    int ok = 0;

    snprintf(build_script, sizeof(build_script), "%s/scripts/build-theme.sh", repo);
    if (access(build_script, X_OK) != 0) {
        append_log(paths, "Icon theme repo non pronta: %s", repo);
        return 0;
    }

    snprintf(build_template, sizeof(build_template), "/tmp/anto426-icon-theme-%ld-XXXXXX", (long)getpid());
    build_root = mkdtemp(build_template);
    if (!build_root) {
        append_log(paths, "Icon theme temp dir non creato");
        return 0;
    }
    snprintf(source_theme, sizeof(source_theme), "%s/%s", build_root, theme_name);

    qrepo = shell_quote(repo);
    qtheme = shell_quote(theme_name);
    qbuild = shell_quote(build_root);
    qstyle = shell_quote(style);
    qvariant = shell_quote(variant);
    qfg = shell_quote(foreground);
    qsurface = shell_quote(surface);
    qaccent = shell_quote(accent);
    qselect = shell_quote(select);
    qborder = shell_quote(border);

    if (!qrepo || !qtheme || !qbuild || !qstyle || !qvariant || !qfg || !qsurface || !qaccent || !qselect || !qborder) {
        goto cleanup;
    }

    snprintf(cmd, sizeof(cmd),
             "cd %s && "
             "ANTO426_ICON_THEME=%s "
             "ANTO426_ICON_OUTPUT_ROOT=%s "
             "ANTO426_MATERIAL_STYLE=%s "
             "ANTO426_MATERIAL_VARIANT=%s "
             "ANTO426_ICON_FILL=%s "
             "ANTO426_ICON_SURFACE=%s "
             "ANTO426_ICON_ACCENT=%s "
             "ANTO426_ICON_SELECT=%s "
             "ANTO426_ICON_BORDER=%s "
             "./scripts/build-theme.sh >/dev/null",
             qrepo, qtheme, qbuild, qstyle, qvariant, qfg, qsurface, qaccent, qselect, qborder);

    if (!run_shell(cmd) || !dir_exists(source_theme)) {
        append_log(paths, "Icon theme build fallito: %s", repo);
        goto cleanup;
    }

    ensure_parent_dir(theme_dir);
    if (command_exists("rsync")) {
        char source_slash[PATH_MAX];
        char *qsource;
        char *qtarget;
        snprintf(source_slash, sizeof(source_slash), "%s/", source_theme);
        qsource = shell_quote(source_slash);
        qtarget = shell_quote(theme_dir);
        if (qsource && qtarget) {
            mkdir_p(theme_dir);
            snprintf(cmd, sizeof(cmd), "rsync -a --delete %s %s/", qsource, qtarget);
            ok = run_shell(cmd);
        }
        free(qsource);
        free(qtarget);
    } else {
        char *qsource = shell_quote(source_theme);
        char *qtarget = shell_quote(theme_dir);
        if (qsource && qtarget) {
            snprintf(cmd, sizeof(cmd), "rm -rf %s && cp -a %s %s", qtarget, qsource, qtarget);
            ok = run_shell(cmd);
        }
        free(qsource);
        free(qtarget);
    }

cleanup:
    {
        char *qtmp = shell_quote(build_root);
        if (qtmp) {
            snprintf(cmd, sizeof(cmd), "rm -rf %s", qtmp);
            run_shell(cmd);
            free(qtmp);
        }
    }
    free(qrepo);
    free(qtheme);
    free(qbuild);
    free(qstyle);
    free(qvariant);
    free(qfg);
    free(qsurface);
    free(qaccent);
    free(qselect);
    free(qborder);
    return ok;
}

static int icons_main(int argc, char **argv) {
    Paths paths;
    char expected[PATH_MAX] = "";
    char theme_name[256];
    char theme_dir[PATH_MAX];
    char repo[PATH_MAX];
    char style[64];
    char variant[64];
    const char *foreground;
    const char *surface;
    const char *accent;
    const char *select;
    const char *border;
    const char *selected_fg;
    char signature[32];
    char old_signature[128] = "";
    char signature_file[PATH_MAX];
    long long start_ms = monotonic_ms();
    long long done_ms;

    init_paths(&paths, "wallpaper_icons.log");
    ensure_runtime_dirs(&paths);

    if (argc > 0 && (strcmp(argv[0], "-h") == 0 || strcmp(argv[0], "--help") == 0)) {
        printf("Usage: wallpaper_core icons expected foreground surface accent select border selected_fg\n");
        return 0;
    }
    if (argc < 7) {
        fprintf(stderr, "Usage: wallpaper_core icons expected foreground surface accent select border selected_fg\n");
        return 2;
    }

    normalize_existing_path(argv[0], expected, sizeof(expected));
    foreground = argv[1];
    surface = argv[2];
    accent = argv[3];
    select = argv[4];
    border = argv[5];
    selected_fg = argv[6];

    if (effects_job_is_stale(&paths, expected)) {
        append_log(&paths, "Icon theme skipped stale job: expected=%s", expected);
        return 0;
    }

    get_icon_paths(&paths, theme_name, sizeof(theme_name), theme_dir, sizeof(theme_dir),
                   repo, sizeof(repo), style, sizeof(style), variant, sizeof(variant));
    icon_signature(theme_name, theme_dir, repo, style, variant, foreground, surface, accent, select, border, selected_fg,
                   signature, sizeof(signature));
    snprintf(signature_file, sizeof(signature_file), "%s/icon-theme.signature", paths.state_dir);

    if (!env_is_on("ANTO426_ICON_THEME_FORCE", 0) &&
        read_first_line(signature_file, old_signature, sizeof(old_signature)) &&
        strcmp(signature, old_signature) == 0) {
        append_log(&paths, "Icon theme skipped: palette e stile invariati");
        return 0;
    }

    if (!sync_icon_theme_from_repo_c(&paths, theme_name, theme_dir, repo, style, variant,
                                     foreground, surface, accent, select, border)) {
        append_log(&paths, "Icon theme non aggiornato: controlla %s", repo);
        return 0;
    }

    if (effects_job_is_stale(&paths, expected)) {
        append_log(&paths, "Icon theme generated but stale before wlogout/cache: expected=%s", expected);
        return 0;
    }

    write_wlogout_material_icons_c(&paths, repo, style, variant, foreground, selected_fg);
    if (command_exists("gtk-update-icon-cache")) {
        char *qtheme_dir = shell_quote(theme_dir);
        char cmd[PATH_MAX + 128];
        if (qtheme_dir) {
            snprintf(cmd, sizeof(cmd), "gtk-update-icon-cache -q -f %s >/dev/null 2>&1 || true", qtheme_dir);
            run_shell(cmd);
            free(qtheme_dir);
        }
    }
    write_text_file(signature_file, "%s\n", signature);
    done_ms = monotonic_ms();
    append_log(&paths, "Icon theme updated in %lld ms: %s -> %s",
               start_ms > 0 && done_ms > 0 ? done_ms - start_ms : -1,
               repo, theme_name);
    return 0;
}

static void start_icon_theme_update(const Paths *paths, const Palette *p, const char *expected) {
    const char *icons_env = getenv("ANTO426_WALLPAPER_CORE_ICONS");
    const char *delay_env = getenv("ANTO426_ICON_THEME_DELAY");
    int delay = 2;
    char core_path[PATH_MAX];
    char icon_log[PATH_MAX];
    char *qcore, *qexpected, *qfg, *qsurface, *qaccent, *qselect, *qborder, *qselected_fg, *qlog;
    char cmd[PATH_MAX * 8 + 1024];

    if (!env_is_on("ANTO426_WALLPAPER_CORE_ICONS", 1)) {
        append_log(paths, "Icon theme C worker skipped by ANTO426_WALLPAPER_CORE_ICONS=%s", icons_env ? icons_env : "");
        return;
    }
    if (delay_env && delay_env[0]) {
        delay = atoi(delay_env);
        if (delay < 0) {
            delay = 0;
        }
        if (delay > 60) {
            delay = 60;
        }
    }

    snprintf(core_path, sizeof(core_path), "%s/wallpaper_core", paths->script_dir);
    if (!regular_file_exists(core_path)) {
        append_log(paths, "Icon theme C worker missing: %s", core_path);
        return;
    }
    snprintf(icon_log, sizeof(icon_log), "%s/wallpaper_icons.log", paths->state_dir);

    qcore = shell_quote(core_path);
    qexpected = shell_quote(expected);
    qfg = shell_quote(p->foreground.hex);
    qsurface = shell_quote(p->surface.hex);
    qaccent = shell_quote(p->accent.hex);
    qselect = shell_quote(p->select.hex);
    qborder = shell_quote(p->border.hex);
    qselected_fg = shell_quote(p->selected_fg.hex);
    qlog = shell_quote(icon_log);

    if (!qcore || !qexpected || !qfg || !qsurface || !qaccent || !qselect || !qborder || !qselected_fg || !qlog) {
        free(qcore); free(qexpected); free(qfg); free(qsurface); free(qaccent);
        free(qselect); free(qborder); free(qselected_fg); free(qlog);
        return;
    }

    snprintf(cmd, sizeof(cmd),
             "( sleep %d; if command -v ionice >/dev/null 2>&1; then "
             "ionice -c3 nice -n 12 %s icons %s %s %s %s %s %s %s; "
             "else nice -n 12 %s icons %s %s %s %s %s %s %s; fi ) >>%s 2>&1 &",
             delay,
             qcore, qexpected, qfg, qsurface, qaccent, qselect, qborder, qselected_fg,
             qcore, qexpected, qfg, qsurface, qaccent, qselect, qborder, qselected_fg,
             qlog);
    if (run_shell(cmd)) {
        append_log(paths, "Icon theme C worker started in background: %s", icon_log);
    } else {
        append_log(paths, "Icon theme C worker start failed");
    }

    free(qcore); free(qexpected); free(qfg); free(qsurface); free(qaccent);
    free(qselect); free(qborder); free(qselected_fg); free(qlog);
}

static int effects_job_is_stale(const Paths *paths, const char *expected) {
    char state_path[PATH_MAX];
    char active[PATH_MAX];
    char active_norm[PATH_MAX];
    char expected_norm[PATH_MAX];

    if (!expected || !expected[0]) {
        return 0;
    }
    snprintf(state_path, sizeof(state_path), "%s/current-wallpaper.path", paths->cache_awww);
    if (!read_first_line(state_path, active, sizeof(active))) {
        return 0;
    }
    normalize_existing_path(active, active_norm, sizeof(active_norm));
    normalize_existing_path(expected, expected_norm, sizeof(expected_norm));
    return strcmp(active_norm, expected_norm) != 0;
}

static void reload_desktop(const Paths *paths, const char *current_wallpaper_path,
                           const char *expected_wallpaper_path) {
    char reload_script[PATH_MAX];
    char cmd[PATH_MAX * 2 + 128];
    char *qreload = NULL;
    char *qexpected = NULL;
    char notify_body[PATH_MAX + 160];
    const char *base = strrchr(current_wallpaper_path, '/');

    snprintf(reload_script, sizeof(reload_script), "%s/theme_reload.sh", paths->script_dir);
    if (regular_file_exists(reload_script)) {
        qreload = shell_quote(reload_script);
        qexpected = shell_quote(expected_wallpaper_path);
        if (qreload && qexpected) {
            snprintf(cmd, sizeof(cmd), "%s %s", qreload, qexpected);
            if (!run_shell(cmd)) {
                append_log(paths, "Targeted theme reload failed: %s", reload_script);
            }
        }
    } else {
        append_log(paths, "Targeted theme reload skipped, missing script: %s", reload_script);
    }
    free(qreload);
    free(qexpected);

    base = base ? base + 1 : current_wallpaper_path;
    snprintf(notify_body, sizeof(notify_body),
             "From %s. Palette applied without rebuilding the desktop session.", base);
    notify_send("Theme updated", notify_body);
}

typedef int EffectLock;
static void release_effect_lock(EffectLock *descriptor) {
    if (*descriptor >= 0) close(*descriptor);
}
G_DEFINE_AUTO_CLEANUP_CLEAR_FUNC(EffectLock, release_effect_lock)

static int acquire_effect_lock(const Paths *paths) {
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/desktop-theme.lock", paths->state_dir);
    int descriptor = open(path, O_CREAT | O_RDWR | O_CLOEXEC | O_NOFOLLOW, 0600);
    if (descriptor < 0) return -1;
    while (flock(descriptor, LOCK_EX) != 0) {
        if (errno == EINTR) continue;
        close(descriptor);
        return -1;
    }
    return descriptor;
}

static int effects_main(int argc, char **argv) {
    Paths paths;
    char source_wallpaper_path[PATH_MAX] = "";
    char current_wallpaper_path[PATH_MAX] = "";
    char expected_norm[PATH_MAX] = "";
    const char *expected_env;
    Canvas canvas;
    char sample_wallpaper_path[PATH_MAX];
    char sample_wallpaper_target[PATH_MAX];
    int r = 0, g = 0, b = 0;
    int ar = 0, ag = 0, ab = 0;
    Palette palette;
    WallpaperPaletteAggregate aggregate;
    char env_path[PATH_MAX];
    long long start_ms = monotonic_ms();
    long long visible_ms;

    init_paths(&paths, "wallpaper_effects.log");
    ensure_runtime_dirs(&paths);

    if (argc > 0 && (strcmp(argv[0], "-h") == 0 || strcmp(argv[0], "--help") == 0)) {
        printf("Usage: wallpaper_core effects [/path/wallpaper]\n");
        return 0;
    }

    if (argc > 0 && argv[0][0]) {
        copy_string(source_wallpaper_path, sizeof(source_wallpaper_path), argv[0]);
    } else if (!detect_current_wallpaper(&paths, source_wallpaper_path, sizeof(source_wallpaper_path))) {
        append_log(&paths, "Invalid wallpaper: empty");
        return 0;
    }

    if (!regular_file_exists(source_wallpaper_path)) {
        append_log(&paths, "Invalid wallpaper: %s", source_wallpaper_path[0] ? source_wallpaper_path : "empty");
        return 0;
    }
    {
        char normalized_source[PATH_MAX];
        normalize_existing_path(source_wallpaper_path, normalized_source,
                                sizeof(normalized_source));
        copy_string(source_wallpaper_path, sizeof(source_wallpaper_path),
                    normalized_source);
    }

    g_auto(EffectLock) effect_lock = acquire_effect_lock(&paths);
    if (effect_lock < 0) {
        append_log(&paths, "Cannot serialize desktop palette generation: %s", strerror(errno));
        return 1;
    }
    expected_env = getenv("ANTO426_WALLPAPER_EFFECTS_EXPECTED");
    if (expected_env && expected_env[0]) {
        normalize_existing_path(expected_env, expected_norm, sizeof(expected_norm));
        if (effects_job_is_stale(&paths, expected_norm)) {
            append_log(&paths, "Skipping stale wallpaper effects: expected=%s", expected_norm);
            return 0;
        }
    }

    if (!command_exists("magick")) {
        append_log(&paths, "Command missing: magick");
        notify_send("Wallpaper", "Command missing: magick");
        return 1;
    }

    canvas = detect_canvas_size();
    if (!collect_active_wallpaper_assignments(&paths, source_wallpaper_path,
                                              &aggregate) ||
        !sample_active_wallpapers(&paths, &aggregate)) {
        append_log(&paths,
                   "Failed to aggregate active wallpaper samples; source=%s",
                   source_wallpaper_path);
        return 1;
    }
    copy_string(current_wallpaper_path, sizeof(current_wallpaper_path),
                aggregate.assets[aggregate.dominant_asset].sample_path);
    r = aggregate.average_r;
    g = aggregate.average_g;
    b = aggregate.average_b;
    ar = aggregate.accent_r;
    ag = aggregate.accent_g;
    ab = aggregate.accent_b;
    append_log(&paths,
               "Updating desktop theme from %zu output(s), %zu unique asset(s) "
               "(%s), dominant=%s",
               aggregate.assignment_count, aggregate.asset_count, canvas.text,
               aggregate.assets[aggregate.dominant_asset].asset);

    /* wallpaper_apply owns current-wallpaper.path. Effects commit only the
       derived cover, after their wallpaper ownership token is still current. */
    snprintf(sample_wallpaper_target, sizeof(sample_wallpaper_target), "%s/normal.png", paths.cache_awww);
    snprintf(sample_wallpaper_path, sizeof(sample_wallpaper_path), "%s/.normal.%ld.tmp.png",
             paths.cache_awww, (long)getpid());
    if (!make_cover_image(current_wallpaper_path, canvas.text, sample_wallpaper_path, "png", 92)) {
        append_log(&paths, "Failed to create cover image: %s", sample_wallpaper_path);
        unlink(sample_wallpaper_path);
        return 1;
    }

    generate_palette(r, g, b, ar, ag, ab, &palette);
    if (effects_job_is_stale(&paths, expected_norm)) {
        append_log(&paths, "Skipping stale palette commit: expected=%s", expected_norm);
        unlink(sample_wallpaper_path);
        return 0;
    }
    if (rename(sample_wallpaper_path, sample_wallpaper_target) != 0) {
        append_log(&paths, "Failed to commit cover image: %s", sample_wallpaper_target);
        unlink(sample_wallpaper_path);
        return 1;
    }
    write_core_theme_parallel(&paths, &palette, current_wallpaper_path);

    write_desktop_palette_state(&paths, &aggregate, &palette);

    write_module_env(&paths, &palette, &canvas, source_wallpaper_path, current_wallpaper_path, env_path, sizeof(env_path));

    if (effects_job_is_stale(&paths, expected_norm)) {
        append_log(&paths, "Skipping stale reload after core write: expected=%s", expected_norm);
        if (env_path[0]) unlink(env_path);
        return 0;
    }

    reload_desktop(&paths, current_wallpaper_path, source_wallpaper_path);
    visible_ms = monotonic_ms();
    if (env_path[0]) {
        start_modules_bridge(&paths, env_path);
    }
    start_icon_theme_update(&paths, &palette, source_wallpaper_path);
    append_log(&paths, "Theme visible in %lld ms: bg=%s surface=%s accent=%s border=%s",
               start_ms > 0 && visible_ms > 0 ? visible_ms - start_ms : -1,
               palette.background.hex, palette.surface.hex, palette.accent.hex, palette.border.hex);
    return 0;
}

static void expand_user_path(const Paths *paths, const char *input, char *out, size_t out_size) {
    if (starts_with(input, "~/")) {
        snprintf(out, out_size, "%s/%s", paths->home, input + 2);
    } else {
        copy_string(out, out_size, input);
    }
}

static const char *palette_token_hex(const Palette *p, const char *token) {
    if (strcmp(token, "background") == 0) return p->background.hex;
    if (strcmp(token, "surface") == 0) return p->surface.hex;
    if (strcmp(token, "select") == 0) return p->select.hex;
    if (strcmp(token, "border") == 0) return p->border.hex;
    if (strcmp(token, "foreground") == 0) return p->foreground.hex;
    if (strcmp(token, "muted") == 0) return p->muted.hex;
    if (strcmp(token, "red") == 0) return p->red.hex;
    if (strcmp(token, "orange") == 0) return p->orange.hex;
    if (strcmp(token, "yellow") == 0) return p->yellow.hex;
    if (strcmp(token, "green") == 0) return p->green.hex;
    if (strcmp(token, "pink") == 0) return p->pink.hex;
    if (strcmp(token, "purple") == 0) return p->purple.hex;
    if (strcmp(token, "gray") == 0) return p->gray.hex;
    return p->accent.hex;
}

static int preview_color_main(int argc, char **argv) {
    Paths paths;
    Canvas canvas;
    Palette palette;
    char source[PATH_MAX];
    char normalized[PATH_MAX];
    char sample_source[PATH_MAX];
    char video_thumb[PATH_MAX] = "";
    char sample_wallpaper_path[PATH_MAX];
    const char *token = "accent";
    int cleanup_video_thumb = 0;
    int r = 0, g = 0, b = 0;
    int ar = 0, ag = 0, ab = 0;
    unsigned long long hash;
    long pid = (long)getpid();

    init_paths(&paths, "wallpaper_preview.log");
    ensure_runtime_dirs(&paths);

    if (argc <= 0 || strcmp(argv[0], "-h") == 0 || strcmp(argv[0], "--help") == 0) {
        fprintf(argc <= 0 ? stderr : stdout, "Usage: wallpaper_core preview-color /path/wallpaper [token]\n");
        return argc <= 0 ? 2 : 0;
    }

    if (argc > 1 && argv[1][0]) {
        token = argv[1];
    }

    expand_user_path(&paths, argv[0], source, sizeof(source));
    if (!regular_file_exists(source)) {
        fprintf(stderr, "Invalid wallpaper: %s\n", source[0] ? source : "empty");
        return 1;
    }
    normalize_existing_path(source, normalized, sizeof(normalized));
    copy_string(source, sizeof(source), normalized);
    copy_string(sample_source, sizeof(sample_source), source);

    if (!command_exists("magick")) {
        fprintf(stderr, "Command missing: magick\n");
        return 1;
    }

    hash = fnv1a_hash(source);
    if (is_video_file(source)) {
        if (!command_exists("ffmpeg")) {
            fprintf(stderr, "Command missing: ffmpeg\n");
            return 1;
        }
        snprintf(video_thumb, sizeof(video_thumb), "%s/preview-video-%016llx-%ld.png", paths.cache_awww, hash, pid);
        if (!extract_video_thumbnail_to(source, video_thumb)) {
            fprintf(stderr, "Failed to extract video thumbnail: %s\n", source);
            return 1;
        }
        copy_string(sample_source, sizeof(sample_source), video_thumb);
        cleanup_video_thumb = 1;
    }

    canvas = detect_canvas_size();
    snprintf(sample_wallpaper_path, sizeof(sample_wallpaper_path), "%s/preview-cover-%016llx-%ld.png", paths.cache_awww, hash, pid);
    if (!make_cover_image(sample_source, canvas.text, sample_wallpaper_path, "png", 92)) {
        if (cleanup_video_thumb) unlink(video_thumb);
        fprintf(stderr, "Failed to create preview cover: %s\n", source);
        return 1;
    }

    if (!extract_average_rgb(sample_wallpaper_path, &r, &g, &b)) {
        unlink(sample_wallpaper_path);
        if (cleanup_video_thumb) unlink(video_thumb);
        fprintf(stderr, "Failed to extract average RGB: %s\n", source);
        return 1;
    }
    if (!extract_vibrant_rgb(sample_wallpaper_path, &ar, &ag, &ab)) {
        ar = r;
        ag = g;
        ab = b;
    }

    generate_palette(r, g, b, ar, ag, ab, &palette);
    printf("%s\n", palette_token_hex(&palette, token));

    unlink(sample_wallpaper_path);
    if (cleanup_video_thumb) unlink(video_thumb);
    return 0;
}

static int palette_plan_main(int argc, char **argv) {
    WallpaperPaletteAggregate aggregate;
    FILE *manifest;
    char line[PATH_MAX * 2];
    Color average;
    Color accent;

    if (argc <= 0 || strcmp(argv[0], "-h") == 0 ||
        strcmp(argv[0], "--help") == 0) {
        fprintf(argc <= 0 ? stderr : stdout,
                "Usage: wallpaper_core palette-plan manifest.tsv\n"
                "Columns: output, width, height, scale, asset, average_hex, "
                "accent_hex\n");
        return argc <= 0 ? 2 : 0;
    }
    manifest = fopen(argv[0], "r");
    if (!manifest) {
        fprintf(stderr, "Cannot read palette manifest: %s\n", argv[0]);
        return 1;
    }
    memset(&aggregate, 0, sizeof(aggregate));
    while (aggregate.assignment_count < MAX_WALLPAPER_OUTPUTS &&
           fgets(line, sizeof(line), manifest)) {
        char *fields[7];
        char *save = NULL;
        char *field;
        size_t field_count = 0;
        char *end = NULL;
        long width;
        long height;
        double scale;
        WallpaperAssignment *assignment;

        if (line[0] == '#') continue;
        for (field = strtok_r(line, "\t\r\n", &save);
             field && field_count < 7;
             field = strtok_r(NULL, "\t\r\n", &save)) {
            fields[field_count++] = field;
        }
        if (field_count != 7) continue;
        width = strtol(fields[1], &end, 10);
        if (!end || *end != '\0' || width <= 0) continue;
        height = strtol(fields[2], &end, 10);
        if (!end || *end != '\0' || height <= 0) continue;
        scale = strtod(fields[3], &end);
        if (!end || *end != '\0' || scale <= 0.0) continue;
        assignment =
            &aggregate.assignments[aggregate.assignment_count];
        memset(assignment, 0, sizeof(*assignment));
        copy_string(assignment->output, sizeof(assignment->output), fields[0]);
        copy_string(assignment->kind, sizeof(assignment->kind), "image");
        copy_string(assignment->asset, sizeof(assignment->asset), fields[4]);
        if (!output_name_is_safe(assignment->output) ||
            !assignment->asset[0] ||
            !parse_rgb_hex(fields[5], &assignment->avg_r,
                           &assignment->avg_g, &assignment->avg_b) ||
            !parse_rgb_hex(fields[6], &assignment->accent_r,
                           &assignment->accent_g,
                           &assignment->accent_b)) {
            continue;
        }
        assignment->logical_area =
            ((double)width / scale) * ((double)height / scale);
        assignment->sampled = 1;
        aggregate.assignment_count++;
    }
    fclose(manifest);
    if (!aggregate_wallpaper_samples(&aggregate)) {
        fprintf(stderr, "Palette manifest contains no valid output\n");
        return 1;
    }

    set_color(&average, aggregate.average_r, aggregate.average_g,
              aggregate.average_b);
    set_color(&accent, aggregate.accent_r, aggregate.accent_g,
              aggregate.accent_b);
    printf("output_count=%zu\nasset_count=%zu\n", aggregate.assignment_count,
           aggregate.asset_count);
    for (size_t index = 0; index < aggregate.assignment_count; index++) {
        WallpaperAssignment *assignment = &aggregate.assignments[index];
        printf("output\t%s\t%s\t%.6f\n", assignment->output,
               assignment->asset, assignment->logical_area);
    }
    for (size_t index = 0; index < aggregate.asset_count; index++) {
        printf("asset\t%s\t%.6f\n", aggregate.assets[index].asset,
               aggregate.assets[index].weight);
    }
    printf("average=%s\naccent=%s\n", average.hex, accent.hex);
    return 0;
}

static void fprint_palette_values(FILE *stream, const Palette *palette) {
    fprint_shell_var(stream, "background", palette->background.hex);
    fprint_shell_var(stream, "surface", palette->surface.hex);
    fprint_shell_var(stream, "select", palette->select.hex);
    fprint_shell_var(stream, "accent", palette->accent.hex);
    fprint_shell_var(stream, "border", palette->border.hex);
    fprint_shell_var(stream, "foreground", palette->foreground.hex);
    fprint_shell_var(stream, "muted", palette->muted.hex);
    fprint_shell_var(stream, "red", palette->red.hex);
    fprint_shell_var(stream, "orange", palette->orange.hex);
    fprint_shell_var(stream, "yellow", palette->yellow.hex);
    fprint_shell_var(stream, "green", palette->green.hex);
    fprint_shell_var(stream, "pink", palette->pink.hex);
    fprint_shell_var(stream, "purple", palette->purple.hex);
    fprint_shell_var(stream, "gray", palette->gray.hex);
    fprint_shell_var(stream, "base", palette->base.hex);
    fprint_shell_var(stream, "base_alt", palette->base_alt.hex);
    fprint_shell_var(stream, "titlebar", palette->titlebar.hex);
    fprint_shell_var(stream, "titlebar_backdrop",
                     palette->titlebar_backdrop.hex);
    fprint_shell_var(stream, "popover", palette->popover.hex);
    fprint_shell_var(stream, "selected_fg", palette->selected_fg.hex);
}

static int palette_samples_main(int argc, char **argv) {
    if (argc != 6) return 2;
    int values[6];
    for (int i = 0; i < 6; i++) {
        char *end = NULL;
        errno = 0;
        long value = strtol(argv[i], &end, 10);
        if (errno || !*argv[i] || *end || value < 0 || value > 255) return 2;
        values[i] = (int)value;
    }
    Palette palette;
    generate_palette(values[0], values[1], values[2], values[3], values[4], values[5], &palette);
    fprint_palette_values(stdout, &palette);
    return 0;
}

static int write_boot_module_env(const Paths *paths, const Palette *palette,
                                 const Canvas *canvas, const char *asset,
                                 const char *sample_path,
                                 const char *generation_dir, char *env_path,
                                 size_t env_path_size) {
    FILE *stream;
    char grub_dir[PATH_MAX];
    char sddm_dir[PATH_MAX];
    char target[PATH_MAX];

    snprintf(env_path, env_path_size, "%s/boot-login-%ld.env",
             paths->state_dir, (long)getpid());
    ensure_parent_dir(env_path);
    stream = fopen(env_path, "w");
    if (!stream) return 0;
    fputs("# Transaction input for the boot/login-only renderer\n",
          stream);
    fputs("effects_domain=boot-login\n", stream);
    fprint_palette_values(stream, palette);
    fprint_shell_var(stream, "source_wallpaper_path", asset);
    fprint_shell_var(stream, "current_wallpaper_path", sample_path);
    fprint_shell_var(stream, "destination_wallpaper_dir", paths->cache_awww);
    fprint_shell_var(stream, "state_dir", paths->state_dir);
    fprint_shell_var(stream, "log_file", paths->log_file);
    fprint_shell_var(stream, "boot_theme_stage_dir", generation_dir);
    fprint_shell_var(stream, "grub_theme_template", paths->grub_theme);
    fprint_shell_var(stream, "sddm_theme_template", paths->sddm_theme);
    fprint_shell_var(stream, "canvas_size", canvas->text);
    fprintf(stream, "canvas_width=%d\ncanvas_height=%d\n", canvas->width,
            canvas->height);

    snprintf(grub_dir, sizeof(grub_dir), "%s/grub", generation_dir);
    snprintf(sddm_dir, sizeof(sddm_dir), "%s/sddm", generation_dir);
    fprint_shell_var(stream, "grub_theme_dir", grub_dir);
    snprintf(target, sizeof(target), "%s/background.jpg", grub_dir);
    fprint_shell_var(stream, "grub_background", target);
    snprintf(target, sizeof(target), "%s/theme.txt", grub_dir);
    fprint_shell_var(stream, "grub_theme", target);
    snprintf(target, sizeof(target), "%s/select_c.png", grub_dir);
    fprint_shell_var(stream, "grub_select_c", target);
    snprintf(target, sizeof(target), "%s/select_e.png", grub_dir);
    fprint_shell_var(stream, "grub_select_e", target);
    snprintf(target, sizeof(target), "%s/select_w.png", grub_dir);
    fprint_shell_var(stream, "grub_select_w", target);
    fprint_shell_var(stream, "sddm_theme_dir", sddm_dir);
    snprintf(target, sizeof(target), "%s/Backgrounds/anto426-current.png",
             sddm_dir);
    fprint_shell_var(stream, "sddm_background", target);
    snprintf(target, sizeof(target), "%s/theme.conf", sddm_dir);
    fprint_shell_var(stream, "sddm_theme", target);

    if (fflush(stream) != 0 || fsync(fileno(stream)) != 0 ||
        fclose(stream) != 0) {
        unlink(env_path);
        env_path[0] = '\0';
        return 0;
    }
    return 1;
}

static int commit_boot_login_state(const Paths *paths, const Palette *palette,
                                   const char *asset,
                                   const char *generation_dir) {
    char *buffer = NULL;
    size_t size = 0;
    FILE *stream = open_memstream(&buffer, &size);
    int ok = 0;

    if (!stream) return 0;
    fputs("# Persistent, boot/login-only wallpaper domain\n", stream);
    fputs("domain=boot-login\n", stream);
    fputs("algorithm=single-asset-vivid-v1\n", stream);
    fprint_shell_var(stream, "asset", asset);
    fprint_shell_var(stream, "generation_dir", generation_dir);
    fprint_palette_values(stream, palette);
    if (fclose(stream) == 0 && buffer) {
        ok = write_text_file(paths->boot_login_state, "%s", buffer);
    }
    free(buffer);
    return ok;
}

static int point_boot_current_generation(const Paths *paths,
                                         const char *generation_dir) {
    char current_link[PATH_MAX];
    char temporary_link[PATH_MAX + 64];

    snprintf(current_link, sizeof(current_link), "%s/current",
             paths->boot_login_stage_dir);
    snprintf(temporary_link, sizeof(temporary_link), "%s/.current.%ld",
             paths->boot_login_stage_dir, (long)getpid());
    unlink(temporary_link);
    if (symlink(generation_dir, temporary_link) != 0) return 0;
    if (rename(temporary_link, current_link) != 0) {
        unlink(temporary_link);
        return 0;
    }
    return 1;
}

static int boot_generation_is_complete(const char *generation_dir) {
    const char *required[] = {
        ".complete",
        "grub/background.jpg",
        "grub/theme.txt",
        "grub/select_c.png", "grub/select_n.png", "grub/select_ne.png",
        "grub/select_e.png", "grub/select_se.png", "grub/select_s.png",
        "grub/select_sw.png", "grub/select_w.png", "grub/select_nw.png",
        "sddm/Backgrounds/anto426-current.png",
        "sddm/theme.conf",
    };

    for (size_t index = 0; index < sizeof(required) / sizeof(required[0]);
         index++) {
        char rendered[PATH_MAX];
        snprintf(rendered, sizeof(rendered), "%s/%s", generation_dir,
                 required[index]);
        if (!nonempty_file_exists(rendered)) return 0;
    }
    return 1;
}

static unsigned long long boot_renderer_hash(const Paths *paths) {
    unsigned long long hash = 14695981039346656037ULL;
    char path[PATH_MAX], resolved[PATH_MAX], root[PATH_MAX];
    snprintf(path, sizeof(path), "%s/boot_render.py", paths->script_dir);
    if (!realpath(path, resolved)) return 0;
    copy_string(root, sizeof(root), resolved);
    for (int index = 0; index < 3; ++index) {
        char *slash = strrchr(root, '/');
        if (!slash) return 0;
        *slash = '\0';
    }
    const char *files[] = {"support/compat/boot_render.py", "design/tokens.json", "support/wallpaper_effects.d/boot_theme.sh"};
    for (size_t index = 0; index < sizeof(files) / sizeof(files[0]); ++index) {
        snprintf(path, sizeof(path), "%s/%s", root, files[index]);
        FILE *stream = fopen(path, "rb");
        if (!stream) return 0;
        int byte;
        while ((byte = fgetc(stream)) != EOF) { hash ^= (unsigned char)byte; hash *= 1099511628211ULL; }
        fclose(stream);
    }
    return hash;
}

static int boot_main(int argc, char **argv) {
    Paths paths;
    Palette palette;
    Canvas canvas;
    char asset[PATH_MAX];
    char normalized[PATH_MAX];
    char sample_path[PATH_MAX];
    char generation_root[PATH_MAX];
    char generation_dir[PATH_MAX];
    char complete_file[PATH_MAX];
    char env_path[PATH_MAX] = "";
    char fingerprint[PATH_MAX + 160];
    char command[PATH_MAX * 3 + 256];
    char *qscript = NULL;
    char *qenv = NULL;
    struct stat asset_stat;
    unsigned long long generation_hash;
    int r, g, b, ar, ag, ab;

    init_paths(&paths, "wallpaper_boot_login.log");
    ensure_runtime_dirs(&paths);
    if (argc <= 0 || strcmp(argv[0], "-h") == 0 ||
        strcmp(argv[0], "--help") == 0) {
        fprintf(argc <= 0 ? stderr : stdout,
                "Usage: wallpaper_core boot /path/wallpaper\n");
        return argc <= 0 ? 2 : 0;
    }
    expand_user_path(&paths, argv[0], asset, sizeof(asset));
    if (!regular_file_exists(asset)) {
        fprintf(stderr, "Invalid boot/login wallpaper: %s\n", asset);
        return 1;
    }
    normalize_existing_path(asset, normalized, sizeof(normalized));
    copy_string(asset, sizeof(asset), normalized);
    if (!command_exists("magick") ||
        !palette_sample_source(&paths, asset,
                               is_video_file(asset) ? "live" : "image",
                               sample_path, sizeof(sample_path)) ||
        !extract_average_rgb(sample_path, &r, &g, &b)) {
        append_log(&paths, "Boot/login sample failed: %s", asset);
        return 1;
    }
    if (!extract_vibrant_rgb(sample_path, &ar, &ag, &ab)) {
        ar = r;
        ag = g;
        ab = b;
    }
    generate_palette(r, g, b, ar, ag, ab, &palette);
    canvas = detect_canvas_size();
    if (stat(asset, &asset_stat) != 0) return 1;
    snprintf(fingerprint, sizeof(fingerprint), "%s:%lld:%lld:%dx%d:boot-login-v3:%016llx",
             asset, (long long)asset_stat.st_mtime,
             (long long)asset_stat.st_size, canvas.width, canvas.height, boot_renderer_hash(&paths));
    generation_hash = fnv1a_hash(fingerprint);
    snprintf(generation_root, sizeof(generation_root), "%s/generations",
             paths.boot_login_stage_dir);
    snprintf(generation_dir, sizeof(generation_dir), "%s/%016llx",
             generation_root, generation_hash);
    snprintf(complete_file, sizeof(complete_file), "%s/.complete",
             generation_dir);
    if (mkdir_p(generation_dir) != 0) return 1;

    if (!boot_generation_is_complete(generation_dir)) {
        if (!write_boot_module_env(&paths, &palette, &canvas, asset,
                                   sample_path, generation_dir, env_path,
                                   sizeof(env_path))) {
            return 1;
        }
        qscript = shell_quote(paths.modules_script);
        qenv = shell_quote(env_path);
        if (!qscript || !qenv) {
            free(qscript);
            free(qenv);
            unlink(env_path);
            return 1;
        }
        snprintf(command, sizeof(command),
                 "ANTO426_WALLPAPER_CORE_APPS=0 "
                 "ANTO426_WALLPAPER_CORE_VSCODE=0 "
                 "ANTO426_WALLPAPER_CORE_OBSIDIAN=0 "
                 "ANTO426_WALLPAPER_CORE_BOOT=1 %s %s boot",
                 qscript, qenv);
        free(qscript);
        free(qenv);
        if (!run_shell(command)) {
            append_log(&paths,
                       "Boot/login staging failed; persistent state unchanged");
            unlink(env_path);
            return 1;
        }
        unlink(env_path);
        if (!write_text_file(complete_file,
                             "asset=%s\nalgorithm=boot-login-v3\n", asset)) {
            return 1;
        }
    }
    if (!boot_generation_is_complete(generation_dir)) {
        append_log(&paths,
                   "Boot/login transaction incomplete after render: %s",
                   generation_dir);
        return 1;
    }
    if (!point_boot_current_generation(&paths, generation_dir) ||
        !commit_boot_login_state(&paths, &palette, asset, generation_dir)) {
        append_log(&paths,
                   "Boot/login commit failed; desktop domain untouched");
        return 1;
    }
    append_log(&paths,
               "Boot/login palette staged transactionally: %s -> %s",
               asset, generation_dir);
    printf("%s\n", generation_dir);
    return 0;
}

static int ensure_awww(const Paths *paths) {
    if (!command_exists("awww")) {
        notify_send("Wallpaper", "awww command not found");
        append_log(paths, "awww not found");
        return 0;
    }
    if (!process_is_running("awww-daemon")) {
        run_shell("awww-daemon >/dev/null 2>&1 &");
        usleep(250000);
    }
    return 1;
}

static double env_double(const char *name, double fallback) {
    const char *value = getenv(name);
    char *end = NULL;
    double parsed;
    if (!value || !value[0]) {
        return fallback;
    }
    parsed = strtod(value, &end);
    return end && *end == '\0' ? parsed : fallback;
}

static long long env_ll(const char *name, long long fallback) {
    const char *value = getenv(name);
    char *end = NULL;
    long long parsed;
    if (!value || !value[0]) {
        return fallback;
    }
    parsed = strtoll(value, &end, 10);
    return end && *end == '\0' ? parsed : fallback;
}

static unsigned long long fnv1a_hash(const char *s) {
    unsigned long long h = 1469598103934665603ULL;
    while (s && *s) {
        h ^= (unsigned char)*s++;
        h *= 1099511628211ULL;
    }
    return h;
}

static void prepared_live_wallpaper(const Paths *paths, const char *source, char *out, size_t out_size) {
    double threshold = env_double("ANTO426_WALLPAPER_SHORT_LOOP_THRESHOLD", 0.0);
    double target = env_double("ANTO426_WALLPAPER_SHORT_LOOP_TARGET", 180.0);
    long long max_repeats = env_ll("ANTO426_WALLPAPER_SHORT_LOOP_MAX_REPEATS", 180);
    long long max_bytes = env_ll("ANTO426_WALLPAPER_SHORT_LOOP_MAX_BYTES", 1073741824LL);
    char *qsource;
    char cmd[PATH_MAX * 3 + 512];
    char duration_out[128];
    struct stat st;
    double duration;
    long long repeats = 1;
    char key[PATH_MAX + 256];
    char cache_dir[PATH_MAX];
    char cache_file[PATH_MAX];
    char tmp_file[PATH_MAX];
    unsigned long long hash;

    copy_string(out, out_size, source);
    if (threshold == 0.0 || !command_exists("ffprobe") || !command_exists("ffmpeg")) {
        return;
    }
    if (threshold <= 0.0) threshold = 15.0;
    if (target <= 0.0) target = 180.0;
    if (max_repeats <= 0) max_repeats = 180;
    if (max_bytes <= 0) max_bytes = 1073741824LL;

    qsource = shell_quote(source);
    if (!qsource) {
        return;
    }
    snprintf(cmd, sizeof(cmd), "ffprobe -v error -show_entries format=duration -of default=nk=1:nw=1 %s 2>/dev/null", qsource);
    if (!capture_command(cmd, duration_out, sizeof(duration_out))) {
        free(qsource);
        return;
    }
    duration = strtod(duration_out, NULL);
    if (stat(source, &st) != 0 || duration <= 0.0 || duration > threshold) {
        free(qsource);
        return;
    }

    repeats = (long long)ceil(target / duration);
    if (repeats < 2) repeats = 2;
    if (repeats > max_repeats) repeats = max_repeats;
    if (st.st_size > 0 && st.st_size * repeats > max_bytes) {
        long long by_size = max_bytes / st.st_size;
        if (by_size < 2) by_size = 2;
        if (by_size < repeats) repeats = by_size;
    }
    if (repeats <= 1) {
        free(qsource);
        return;
    }

    snprintf(cache_dir, sizeof(cache_dir), "%s/live-loop-cache", paths->cache_awww);
    mkdir_p(cache_dir);
    snprintf(key, sizeof(key), "%s:%lld:%lld:%lld:%s", source, (long long)st.st_size, (long long)st.st_mtime, repeats, live_options_version);
    hash = fnv1a_hash(key);
    snprintf(cache_file, sizeof(cache_file), "%s/%016llx.mkv", cache_dir, hash);
    if (nonempty_file_exists(cache_file)) {
        copy_string(out, out_size, cache_file);
        free(qsource);
        return;
    }
    snprintf(tmp_file, sizeof(tmp_file), "%s/%016llx.tmp.mkv", cache_dir, hash);
    {
        char *qtmp = shell_quote(tmp_file);
        if (!qtmp) {
            free(qsource);
            return;
        }
        snprintf(cmd, sizeof(cmd),
                 "ffmpeg -hide_banner -loglevel error -y -stream_loop %lld -i %s "
                 "-map 0:v:0 -an -sn -dn -c:v copy -avoid_negative_ts make_zero %s >/dev/null 2>&1",
                 repeats - 1, qsource, qtmp);
        if (run_shell(cmd) && rename(tmp_file, cache_file) == 0) {
            append_log(paths, "prepared lossless short live wallpaper loop cache: %s (%lldx, copy)", cache_file, repeats);
            copy_string(out, out_size, cache_file);
        } else {
            unlink(tmp_file);
        }
        free(qtmp);
    }
    free(qsource);
}

static void start_wallpaper_daemon(const Paths *paths) {
    char *qdaemon;
    char cmd[PATH_MAX + 256];

    if (!regular_file_exists(paths->daemon_script)) {
        return;
    }
    if (run_shell("pgrep -af '[/]wallpaper_daemon([[:space:]]|$)' >/dev/null 2>&1 || pgrep -af '[/]wallpaper_daemon.sh' >/dev/null 2>&1")) {
        return;
    }
    qdaemon = shell_quote(paths->daemon_script);
    if (!qdaemon) {
        return;
    }
    snprintf(cmd, sizeof(cmd), "%s >/dev/null 2>&1 &", qdaemon);
    run_shell(cmd);
    free(qdaemon);
}

static int apply_main(int argc, char **argv) {
    Paths paths;
    char wallpaper[PATH_MAX];
    char normalized[PATH_MAX];
    char state_path[PATH_MAX];
    char output[256];
    char transition[64];
    char duration[64];
    const char *output_env;
    const char *transition_env;
    const char *duration_env;
    int skip_effects;
    int silent;
    int is_video;

    init_paths(&paths, "wallpaper_apply.log");
    ensure_runtime_dirs(&paths);
    output_env = getenv("ANTO426_WALLPAPER_OUTPUT");
    if (!output_env || !output_env[0] || strcmp(output_env, "__all__") == 0 ||
        strcmp(output_env, "*") == 0) {
        copy_string(output, sizeof(output), "ALL");
    } else if (!output_name_is_safe(output_env)) {
        append_log(&paths, "invalid wallpaper output rejected");
        return 2;
    } else {
        copy_string(output, sizeof(output), output_env);
    }
    skip_effects = getenv("ANTO426_WALLPAPER_SKIP_EFFECTS") &&
                   strcmp(getenv("ANTO426_WALLPAPER_SKIP_EFFECTS"), "1") == 0;
    silent = getenv("ANTO426_WALLPAPER_SILENT") &&
             strcmp(getenv("ANTO426_WALLPAPER_SILENT"), "1") == 0;

    if (argc <= 0) {
        fprintf(stderr, "Usage: wallpaper_core apply /path/wallpaper | --restore\n");
        return 2;
    }
    if (strcmp(argv[0], "-h") == 0 || strcmp(argv[0], "--help") == 0) {
        printf("Usage: wallpaper_core apply /path/wallpaper | --restore\n");
        return 0;
    }

    if (strcmp(argv[0], "--restore") == 0) {
        typedef struct {
            char output[256];
            char wallpaper[PATH_MAX];
        } RestoreState;
        RestoreState states[64];
        char output_state_dir[PATH_MAX];
        char saved_wallpaper[PATH_MAX] = "";
        char saved_output[256] = "ALL";
        char output_path[PATH_MAX];
        char *old_output = getenv("ANTO426_WALLPAPER_OUTPUT")
                               ? strdup(getenv("ANTO426_WALLPAPER_OUTPUT"))
                               : NULL;
        char *old_skip = getenv("ANTO426_WALLPAPER_SKIP_EFFECTS")
                             ? strdup(getenv("ANTO426_WALLPAPER_SKIP_EFFECTS"))
                             : NULL;
        char *old_silent = getenv("ANTO426_WALLPAPER_SILENT")
                               ? strdup(getenv("ANTO426_WALLPAPER_SILENT"))
                               : NULL;
        size_t state_count = 0;
        int restore_status = 0;
        DIR *dir;
        struct dirent *entry;

        {
            FILE *current_state = fopen(paths.wallpaper_current_state, "r");
            if (current_state) {
                if (!fgets(saved_output, sizeof(saved_output), current_state) ||
                    !fgets(saved_wallpaper, sizeof(saved_wallpaper),
                           current_state)) {
                    saved_output[0] = '\0';
                    saved_wallpaper[0] = '\0';
                }
                fclose(current_state);
                trim_newline(saved_output);
                trim_newline(saved_wallpaper);
            }
        }
        snprintf(state_path, sizeof(state_path), "%s/current-wallpaper.path",
                 paths.cache_awww);
        snprintf(output_path, sizeof(output_path),
                 "%s/current-wallpaper-output.path", paths.cache_awww);
        if (!saved_wallpaper[0]) {
            read_first_line(state_path, saved_wallpaper,
                            sizeof(saved_wallpaper));
        }
        if (!output_name_is_safe(saved_output)) {
            if (!read_first_line(output_path, saved_output,
                                 sizeof(saved_output)) ||
                !output_name_is_safe(saved_output)) {
                copy_string(saved_output, sizeof(saved_output), "ALL");
            }
        }

        copy_string(output_state_dir, sizeof(output_state_dir),
                    paths.wallpaper_output_dir);
        dir = opendir(output_state_dir);
        if (!dir) {
            /* One-time compatibility read for pre-centralization installs. */
            snprintf(output_state_dir, sizeof(output_state_dir),
                     "%s/output-state", paths.cache_awww);
            dir = opendir(output_state_dir);
        }
        if (dir) {
            while (state_count < sizeof(states) / sizeof(states[0]) &&
                   (entry = readdir(dir)) != NULL) {
                char file_path[PATH_MAX];
                char kind[32];
                FILE *state_file;
                size_t length = strlen(entry->d_name);

                if (length <= 6 ||
                    strcmp(entry->d_name + length - 6, ".state") != 0) {
                    continue;
                }
                snprintf(file_path, sizeof(file_path), "%s/%s", output_state_dir,
                         entry->d_name);
                state_file = fopen(file_path, "r");
                if (!state_file) {
                    continue;
                }
                if (!fgets(states[state_count].output,
                           sizeof(states[state_count].output), state_file) ||
                    !fgets(kind, sizeof(kind), state_file) ||
                    !fgets(states[state_count].wallpaper,
                           sizeof(states[state_count].wallpaper), state_file)) {
                    fclose(state_file);
                    continue;
                }
                fclose(state_file);
                trim_newline(states[state_count].output);
                trim_newline(states[state_count].wallpaper);
                if (!output_name_is_safe(states[state_count].output) ||
                    !regular_file_exists(states[state_count].wallpaper)) {
                    continue;
                }
                state_count++;
            }
            closedir(dir);
        }

        if (state_count == 0) {
            if (regular_file_exists(saved_wallpaper)) {
                char *restore_argv[] = {saved_wallpaper, NULL};
                setenv("ANTO426_WALLPAPER_OUTPUT", saved_output, 1);
                append_log(&paths, "Restoring saved wallpaper: %s on %s",
                           saved_wallpaper, saved_output);
                restore_status = apply_main(1, restore_argv);
            } else {
                append_log(&paths, "No saved wallpaper to restore");
            }
        } else {
            setenv("ANTO426_WALLPAPER_SKIP_EFFECTS", "1", 1);
            setenv("ANTO426_WALLPAPER_SILENT", "1", 1);
            OutputGeometry connected[MAX_WALLPAPER_OUTPUTS];
            size_t connected_count = 0;
            int have_outputs = read_output_geometries(connected, &connected_count);
            for (int pass = 0; pass < 2; pass++) {
                for (size_t i = 0; i < state_count; i++) {
                    int is_all = strcmp(states[i].output, "ALL") == 0;
                    char *restore_argv[] = {states[i].wallpaper, NULL};

                    if ((pass == 0) != is_all) {
                        continue;
                    }
                    if (!is_all && have_outputs) {
                        int available = 0;
                        for (size_t index = 0; index < connected_count; index++)
                            if (strcmp(connected[index].output, states[i].output) == 0) available = 1;
                        if (!available) {
                            append_log(&paths, "Preserving wallpaper for disconnected output: %s", states[i].output);
                            continue;
                        }
                    }
                    setenv("ANTO426_WALLPAPER_OUTPUT", states[i].output, 1);
                    append_log(&paths, "Restoring output wallpaper: %s on %s",
                               states[i].wallpaper, states[i].output);
                    if (apply_main(1, restore_argv) != 0) {
                        restore_status = 1;
                    }
                }
            }

            /*
             * Per-output wallpaper state is independent; the desktop palette
             * remains global and follows the wallpaper selected most recently.
             */
            if (regular_file_exists(saved_wallpaper)) {
                write_text_file(state_path, "%s\n", saved_wallpaper);
                write_text_file(output_path, "%s\n", saved_output);
                write_wallpaper_current_state(&paths, saved_output,
                                              saved_wallpaper);
                if (regular_file_exists(paths.effects_script)) {
                    char *qeffects = shell_quote(paths.effects_script);
                    char *qwall = shell_quote(saved_wallpaper);
                    char cmd[PATH_MAX * 3 + 128];
                    if (qeffects && qwall) {
                        snprintf(cmd, sizeof(cmd),
                                 "ANTO426_WALLPAPER_EFFECTS_EXPECTED=%s %s %s",
                                 qwall, qeffects, qwall);
                        if (!run_shell(cmd)) {
                            append_log(&paths,
                                       "wallpaper_effects failed after per-output restore: %s",
                                       saved_wallpaper);
                        }
                    }
                    free(qeffects);
                    free(qwall);
                }
            }
            append_log(&paths, "Restored %zu wallpaper output state(s)", state_count);
        }

        if (old_output) {
            setenv("ANTO426_WALLPAPER_OUTPUT", old_output, 1);
        } else {
            unsetenv("ANTO426_WALLPAPER_OUTPUT");
        }
        if (old_skip) {
            setenv("ANTO426_WALLPAPER_SKIP_EFFECTS", old_skip, 1);
        } else {
            unsetenv("ANTO426_WALLPAPER_SKIP_EFFECTS");
        }
        if (old_silent) {
            setenv("ANTO426_WALLPAPER_SILENT", old_silent, 1);
        } else {
            unsetenv("ANTO426_WALLPAPER_SILENT");
        }
        free(old_output);
        free(old_skip);
        free(old_silent);
        return restore_status;
    }

    expand_user_path(&paths, argv[0], wallpaper, sizeof(wallpaper));
    if (!regular_file_exists(wallpaper)) {
        char msg[PATH_MAX + 64];
        snprintf(msg, sizeof(msg), "Wallpaper not found: %s", wallpaper);
        notify_send("Wallpaper", msg);
        append_log(&paths, "wallpaper not found: %s", wallpaper);
        return 1;
    }
    normalize_existing_path(wallpaper, normalized, sizeof(normalized));
    copy_string(wallpaper, sizeof(wallpaper), normalized);
    is_video = is_video_file(wallpaper);

    if (is_video) {
        char playback_wallpaper[PATH_MAX];
        char options_file[PATH_MAX];
        char playback_file[PATH_MAX];
        char output_file[PATH_MAX];
        char saved_version[64] = "";
        char ipc_sock[PATH_MAX];
        char *qopts, *qoutput, *qplayback, *qlog;
        char cmd[PATH_MAX * 4 + 2048];
        const char *runtime_dir;
        unsigned long long output_hash;

        if (!command_exists("mpvpaper")) {
            notify_send("Wallpaper", "mpvpaper non e installato per i live wallpaper!");
            append_log(&paths, "mpvpaper not found for video wallpaper: %s", wallpaper);
            return 1;
        }

        snprintf(options_file, sizeof(options_file), "%s/current-live-options.version", paths.cache_awww);
        read_first_line(options_file, saved_version, sizeof(saved_version));
        prepared_live_wallpaper(&paths, wallpaper, playback_wallpaper, sizeof(playback_wallpaper));

        if (strcmp(saved_version, live_options_version) == 0 &&
            mpvpaper_process_matches(output, playback_wallpaper, 0)) {
            append_log(&paths,
                       "live wallpaper already active, skipping mpvpaper restart: %s on %s",
                       wallpaper, output);
            snprintf(state_path, sizeof(state_path), "%s/current-wallpaper.path", paths.cache_awww);
            snprintf(playback_file, sizeof(playback_file), "%s/current-live-playback.path", paths.cache_awww);
            snprintf(output_file, sizeof(output_file), "%s/current-wallpaper-output.path", paths.cache_awww);
            write_text_file(state_path, "%s\n", wallpaper);
            write_text_file(playback_file, "%s\n", playback_wallpaper);
            write_text_file(output_file, "%s\n", output);
            if (strcmp(output, "ALL") == 0) {
                migrate_legacy_wallpaper_output_states(&paths);
                clear_wallpaper_output_states(&paths);
            }
            write_wallpaper_output_state(&paths, output, "live", wallpaper);
            write_wallpaper_current_state(&paths, output, wallpaper);
            start_wallpaper_daemon(&paths);
            return 0;
        }

        stop_mpvpaper_for_output(&paths, output);
        run_shell("pkill swww-daemon >/dev/null 2>&1 || true");

        runtime_dir = getenv("XDG_RUNTIME_DIR");
        output_hash = fnv1a_hash(output);
        snprintf(ipc_sock, sizeof(ipc_sock), "%s/mpvpaper-ipc-%016llx",
                 runtime_dir && runtime_dir[0] ? runtime_dir : "/tmp", output_hash);
        qopts = shell_quote("no-audio loop-file=inf keep-open=yes --panscan=1.0 --hidpi-window-scale=yes --hwdec=auto --osd-level=0 --input-ipc-server=");
        qoutput = shell_quote(output);
        qplayback = shell_quote(playback_wallpaper);
        {
            char mpv_log[PATH_MAX];
            snprintf(mpv_log, sizeof(mpv_log), "%s/mpvpaper-%016llx.log",
                     paths.state_dir, output_hash);
            qlog = shell_quote(mpv_log);
        }
        if (!qopts || !qoutput || !qplayback || !qlog) {
            free(qopts); free(qoutput); free(qplayback); free(qlog);
            return 1;
        }
        char opts_with_sock[PATH_MAX + 256];
        snprintf(opts_with_sock, sizeof(opts_with_sock),
                 "no-audio loop-file=inf keep-open=yes --panscan=1.0 --hidpi-window-scale=yes --hwdec=auto --osd-level=0 --input-ipc-server=%s",
                 ipc_sock);
        free(qopts);
        qopts = shell_quote(opts_with_sock);
        if (!qopts) {
            free(qoutput); free(qplayback); free(qlog);
            return 1;
        }
        snprintf(cmd, sizeof(cmd), "mpvpaper -f -o %s %s %s >%s 2>&1",
                 qopts, qoutput, qplayback, qlog);
        append_log(&paths, "Applying live wallpaper: %s on %s", wallpaper, output);
        if (run_shell(cmd)) {
            snprintf(state_path, sizeof(state_path), "%s/current-wallpaper.path", paths.cache_awww);
            snprintf(playback_file, sizeof(playback_file), "%s/current-live-playback.path", paths.cache_awww);
            snprintf(output_file, sizeof(output_file), "%s/current-wallpaper-output.path", paths.cache_awww);
            write_text_file(state_path, "%s\n", wallpaper);
            write_text_file(playback_file, "%s\n", playback_wallpaper);
            write_text_file(output_file, "%s\n", output);
            write_text_file(options_file, "%s\n", live_options_version);
            if (strcmp(output, "ALL") == 0) {
                migrate_legacy_wallpaper_output_states(&paths);
                clear_wallpaper_output_states(&paths);
            }
            write_wallpaper_output_state(&paths, output, "live", wallpaper);
            write_wallpaper_current_state(&paths, output, wallpaper);
            append_log(&paths, "live wallpaper applied: %s on %s", wallpaper, output);
            start_wallpaper_daemon(&paths);
        } else {
            notify_send("Wallpaper", "Errore nell'avvio di mpvpaper");
            append_log(&paths, "mpvpaper failed: %s on %s", wallpaper, output);
            free(qopts); free(qoutput); free(qplayback); free(qlog);
            return 1;
        }
        free(qopts); free(qoutput); free(qplayback); free(qlog);
    } else {
        char *qwall;
        char *qoutput = NULL;
        char cmd[PATH_MAX * 2 + 512];
        transition_env = getenv("ANTO426_WALLPAPER_TRANSITION");
        duration_env = getenv("ANTO426_WALLPAPER_DURATION");
        copy_string(transition, sizeof(transition), transition_env && transition_env[0] ? transition_env : "any");
        copy_string(duration, sizeof(duration), duration_env && duration_env[0] ? duration_env : "2");

        stop_mpvpaper_for_output(&paths, output);
        if (!ensure_awww(&paths)) {
            return 1;
        }
        qwall = shell_quote(wallpaper);
        if (strcmp(output, "ALL") != 0) {
            qoutput = shell_quote(output);
        }
        if (!qwall || (strcmp(output, "ALL") != 0 && !qoutput)) {
            free(qwall);
            free(qoutput);
            return 1;
        }
        if (strcmp(output, "ALL") == 0) {
            snprintf(cmd, sizeof(cmd),
                     "awww img %s --transition-type %s --transition-duration %s",
                     qwall, transition, duration);
        } else {
            snprintf(cmd, sizeof(cmd),
                     "awww img --outputs %s %s --transition-type %s --transition-duration %s",
                     qoutput, qwall, transition, duration);
        }
        if (run_shell(cmd)) {
            char output_file[PATH_MAX];
            snprintf(state_path, sizeof(state_path), "%s/current-wallpaper.path", paths.cache_awww);
            snprintf(output_file, sizeof(output_file), "%s/current-wallpaper-output.path", paths.cache_awww);
            write_text_file(state_path, "%s\n", wallpaper);
            write_text_file(output_file, "%s\n", output);
            if (strcmp(output, "ALL") == 0) {
                migrate_legacy_wallpaper_output_states(&paths);
                clear_wallpaper_output_states(&paths);
            }
            write_wallpaper_output_state(&paths, output, "image", wallpaper);
            write_wallpaper_current_state(&paths, output, wallpaper);
            append_log(&paths, "wallpaper applied: %s on %s", wallpaper, output);
        } else {
            notify_send("Wallpaper", "Failed to change wallpaper");
            append_log(&paths, "awww img failed: %s on %s", wallpaper, output);
            free(qwall);
            free(qoutput);
            return 1;
        }
        free(qwall);
        free(qoutput);
    }

    if (!skip_effects && regular_file_exists(paths.effects_script)) {
        char *qeffects = shell_quote(paths.effects_script);
        char *qwall = shell_quote(wallpaper);
        char *qexpected = shell_quote(wallpaper);
        char cmd[PATH_MAX * 3 + 128];
        if (qeffects && qwall && qexpected) {
            snprintf(cmd, sizeof(cmd), "ANTO426_WALLPAPER_EFFECTS_EXPECTED=%s %s %s", qexpected, qeffects, qwall);
            if (!run_shell(cmd)) {
                append_log(&paths, "wallpaper_effects failed: %s", wallpaper);
            }
        }
        free(qeffects);
        free(qwall);
        free(qexpected);
    } else if (!skip_effects) {
        append_log(&paths, "wallpaper_effects not executable: %s", paths.effects_script);
    }

    if (!silent) {
        const char *base = strrchr(wallpaper, '/');
        char msg[PATH_MAX + 64];
        base = base ? base + 1 : wallpaper;
        snprintf(msg, sizeof(msg), "Wallpaper applied: %s · %s", base, output);
        notify_send("Wallpaper", msg);
    }
    return 0;
}

static void usage(FILE *stream) {
    fprintf(stream,
            "Usage:\n"
            "  wallpaper_core apply /path/wallpaper | --restore\n"
            "  wallpaper_core effects [/path/wallpaper]\n"
            "  wallpaper_core boot /path/wallpaper\n"
            "  wallpaper_core icons expected foreground surface accent select border selected_fg\n"
            "  wallpaper_core preview-color /path/wallpaper [token]\n"
            "  wallpaper_core palette-plan manifest.tsv\n"
            "  wallpaper_core palette-samples r g b accent_r accent_g accent_b\n");
}

void wallpaper_load_preferences(void);
int main(int argc, char **argv) {
    wallpaper_load_preferences();
    if (argc < 2 || strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
        usage(argc < 2 ? stderr : stdout);
        return argc < 2 ? 2 : 0;
    }
    if (strcmp(argv[1], "apply") == 0) {
        return apply_main(argc - 2, argv + 2);
    }
    if (strcmp(argv[1], "effects") == 0) {
        return effects_main(argc - 2, argv + 2);
    }
    if (strcmp(argv[1], "boot") == 0) {
        return boot_main(argc - 2, argv + 2);
    }
    if (strcmp(argv[1], "icons") == 0) {
        return icons_main(argc - 2, argv + 2);
    }
    if (strcmp(argv[1], "preview-color") == 0) {
        return preview_color_main(argc - 2, argv + 2);
    }
    if (strcmp(argv[1], "palette-samples") == 0) {
        return palette_samples_main(argc - 2, argv + 2);
    }
    if (strcmp(argv[1], "palette-plan") == 0) {
        return palette_plan_main(argc - 2, argv + 2);
    }
    usage(stderr);
    return 2;
}
