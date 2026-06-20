#include <gtk/gtk.h>
#include <gtk-layer-shell.h>
#include <glib-unix.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#define OSD_WIDTH 380
#define OSD_HEIGHT 64
#define BAR_WIDTH 260
#define BOTTOM_MARGIN 48
#define HIDE_MS 1400

static char state_file_path[1024];
static char pid_file_path[1024];
static char colors_file_path[1024];
static char style_file_path[1024];

static char state_kind[32] = "volume";
static int state_value = 0;
static int state_muted = 0;

static GtkWidget *icon_label = NULL;
static GtkWidget *title_label = NULL;
static GtkWidget *value_label = NULL;
static GtkWidget *progress_bar = NULL;
static guint hide_timeout_id = 0;

static const char *FALLBACK_CSS = 
    "@define-color background #1e1e2e;\n"
    "@define-color surface #313244;\n"
    "@define-color foreground #f6f7fb;\n"
    "@define-color muted #b9c4d2;\n"
    "@define-color accent #8cb8e4;\n"
    "@define-color border #6c7086;\n"
    "@define-color panel-bg rgba(30, 30, 46, 0.62);\n"
    "@define-color overlay-bg rgba(30, 30, 46, 0.28);\n"
    "@define-color item-bg rgba(49, 50, 68, 0.18);\n"
    "@define-color item-bg-active rgba(140, 184, 228, 0.42);\n"
    "@define-color border-medium rgba(108, 112, 134, 0.34);\n"
    "@define-color background-alpha @panel-bg;\n"
    "@define-color surface-alpha @item-bg;\n";

static void resolve_paths(void) {
    // 1. STATE_FILE
    const char *env_state = getenv("ANTO426_OSD_STATE");
    if (env_state) {
        strncpy(state_file_path, env_state, sizeof(state_file_path) - 1);
    } else {
        const char *runtime_dir = getenv("XDG_RUNTIME_DIR");
        if (runtime_dir) {
            snprintf(state_file_path, sizeof(state_file_path), "%s/anto426-osd.state", runtime_dir);
        } else {
            strcpy(state_file_path, "/tmp/anto426-osd.state");
        }
    }

    // 2. PID_FILE
    const char *env_pid = getenv("ANTO426_OSD_PID");
    if (env_pid) {
        strncpy(pid_file_path, env_pid, sizeof(pid_file_path) - 1);
    } else {
        const char *runtime_dir = getenv("XDG_RUNTIME_DIR");
        if (runtime_dir) {
            snprintf(pid_file_path, sizeof(pid_file_path), "%s/anto426-osd.pid", runtime_dir);
        } else {
            strcpy(pid_file_path, "/tmp/anto426-osd.pid");
        }
    }

    // 3. COLORS_FILE
    const char *config_home = getenv("XDG_CONFIG_HOME");
    if (config_home) {
        snprintf(colors_file_path, sizeof(colors_file_path), "%s/colors/colors.css", config_home);
    } else {
        const char *home = getenv("HOME");
        if (home) {
            snprintf(colors_file_path, sizeof(colors_file_path), "%s/.config/colors/colors.css", home);
        } else {
            strcpy(colors_file_path, "/tmp/colors.css");
        }
    }

    // 4. CSS_FILE (style.css in the same directory as the binary)
    char exe_path[1024];
    ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
    if (len != -1) {
        exe_path[len] = '\0';
        char *dir = strrchr(exe_path, '/');
        if (dir) {
            *dir = '\0';
            snprintf(style_file_path, sizeof(style_file_path), "%s/style.css", exe_path);
        } else {
            strcpy(style_file_path, "style.css");
        }
    } else {
        strcpy(style_file_path, "style.css");
    }
}

static void read_state(void) {
    FILE *f = fopen(state_file_path, "r");
    if (!f) return;
    if (fscanf(f, "%31s %d %d", state_kind, &state_value, &state_muted) < 2) {
        strcpy(state_kind, "volume");
        state_value = 0;
        state_muted = 0;
    }
    fclose(f);
}

static void write_state(const char *kind, int value, int muted) {
    FILE *f = fopen(state_file_path, "w");
    if (f) {
        fprintf(f, "%s %d %d\n", kind, value, muted);
        fclose(f);
    }
}

static void write_pid_file(void) {
    FILE *f = fopen(pid_file_path, "w");
    if (f) {
        fprintf(f, "%d\n", getpid());
        fclose(f);
    }
}

static void cleanup_pid_file(void) {
    FILE *f = fopen(pid_file_path, "r");
    if (f) {
        int read_pid = 0;
        if (fscanf(f, "%d", &read_pid) == 1) {
            if (read_pid == getpid()) {
                unlink(pid_file_path);
            }
        }
        fclose(f);
    }
}

static void configure_surface(GtkWidget *window) {
    GdkScreen *screen = gtk_window_get_screen(GTK_WINDOW(window));
    GdkVisual *visual = gdk_screen_get_rgba_visual(screen);
    if (visual) {
        gtk_widget_set_visual(window, visual);
    }

    if (gtk_layer_is_supported()) {
        gtk_layer_init_for_window(GTK_WINDOW(window));
        gtk_layer_set_namespace(GTK_WINDOW(window), "anto426-osd");
        gtk_layer_set_layer(GTK_WINDOW(window), GTK_LAYER_SHELL_LAYER_OVERLAY);
        gtk_layer_set_keyboard_mode(GTK_WINDOW(window), GTK_LAYER_SHELL_KEYBOARD_MODE_NONE);
        gtk_layer_set_exclusive_zone(GTK_WINDOW(window), -1);
        gtk_layer_set_anchor(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_LEFT, FALSE);
        gtk_layer_set_anchor(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_RIGHT, FALSE);
        gtk_layer_set_anchor(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_TOP, FALSE);
        gtk_layer_set_anchor(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_BOTTOM, TRUE);
    } else {
        g_warning("GtkLayerShell is NOT supported. Running in fallback X11 mode.");
    }
}

static void center_window(GtkWidget *window) {
    if (gtk_layer_is_supported()) {
        gtk_layer_set_margin(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_BOTTOM, BOTTOM_MARGIN);
    } else {
        GdkDisplay *display = gdk_display_get_default();
        GdkMonitor *monitor = NULL;
        if (display) {
            monitor = gdk_display_get_primary_monitor(display);
            if (!monitor) {
                monitor = gdk_display_get_monitor(display, 0);
            }
        }
        if (!monitor) return;

        GdkRectangle geo;
        gdk_monitor_get_geometry(monitor, &geo);

        int x = (geo.width - OSD_WIDTH) / 2;
        if (x < 0) x = 0;
        int y = geo.height - OSD_HEIGHT - BOTTOM_MARGIN;
        gtk_window_move(GTK_WINDOW(window), geo.x + x, geo.y + y);
    }
}

static void load_css(GtkWidget *window) {
    GdkScreen *screen = gtk_window_get_screen(GTK_WINDOW(window));
    if (!screen) return;

    GtkCssProvider *fallback_provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(fallback_provider, FALLBACK_CSS, -1, NULL);
    gtk_style_context_add_provider_for_screen(
        screen,
        GTK_STYLE_PROVIDER(fallback_provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
    );
    g_object_unref(fallback_provider);

    if (access(colors_file_path, F_OK) == 0) {
        GtkCssProvider *colors_provider = gtk_css_provider_new();
        gtk_css_provider_load_from_path(colors_provider, colors_file_path, NULL);
        gtk_style_context_add_provider_for_screen(
            screen,
            GTK_STYLE_PROVIDER(colors_provider),
            GTK_STYLE_PROVIDER_PRIORITY_APPLICATION + 1
        );
        g_object_unref(colors_provider);
    }

    if (access(style_file_path, F_OK) == 0) {
        GtkCssProvider *style_provider = gtk_css_provider_new();
        gtk_css_provider_load_from_path(style_provider, style_file_path, NULL);
        gtk_style_context_add_provider_for_screen(
            screen,
            GTK_STYLE_PROVIDER(style_provider),
            GTK_STYLE_PROVIDER_PRIORITY_APPLICATION + 2
        );
        g_object_unref(style_provider);
    }
}

static gboolean on_hide_timeout(gpointer data) {
    (void)data;
    gtk_main_quit();
    hide_timeout_id = 0;
    return FALSE;
}

static void schedule_hide(void) {
    if (hide_timeout_id > 0) {
        g_source_remove(hide_timeout_id);
    }
    hide_timeout_id = g_timeout_add(HIDE_MS, on_hide_timeout, NULL);
}

static void update_osd(GtkWidget *window) {
    read_state();

    const char *icon_str = "";
    const char *title_str = "";
    static char value_str[32];

    if (strcmp(state_kind, "brightness") == 0) {
        icon_str = "󰃠";
        title_str = "Luminosità";
    } else if (strcmp(state_kind, "mic") == 0) {
        icon_str = state_muted ? "󰝟" : "󰍬";
        title_str = "Microfono";
    } else { // volume
        icon_str = state_muted ? "󰝟" : "󰕾";
        title_str = "Volume";
    }

    if (state_muted) {
        strcpy(value_str, "Muto");
    } else {
        snprintf(value_str, sizeof(value_str), "%d%%", state_value);
    }

    gtk_label_set_text(GTK_LABEL(icon_label), icon_str);
    char *title_upper = g_utf8_strup(title_str, -1);
    gtk_label_set_text(GTK_LABEL(title_label), title_upper);
    g_free(title_upper);
    gtk_label_set_text(GTK_LABEL(value_label), value_str);

    double fraction = state_muted ? 0.0 : ((double)state_value / 100.0);
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progress_bar), fraction);

    GtkStyleContext *context = gtk_widget_get_style_context(progress_bar);
    if (state_muted) {
        gtk_style_context_add_class(context, "muted");
    } else {
        gtk_style_context_remove_class(context, "muted");
    }

    if (!gtk_widget_get_visible(window)) {
        gtk_widget_show_all(window);
    }
    center_window(window);
    schedule_hide();
}

static gboolean on_sigusr1(gpointer user_data) {
    GtkWidget *window = GTK_WIDGET(user_data);
    update_osd(window);
    return TRUE;
}

static gboolean on_draw(GtkWidget *widget, cairo_t *cr, gpointer user_data) {
    (void)widget;
    (void)user_data;
    cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.0);
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_paint(cr);
    return FALSE;
}

int main(int argc, char *argv[]) {
    resolve_paths();

    if (argc >= 2 && strcmp(argv[1], "daemon") != 0) {
        const char *kind = argv[1];
        int value = (argc > 2) ? atoi(argv[2]) : 0;
        int muted = 0;
        if (argc > 3 && (strcmp(argv[3], "1") == 0 || strcmp(argv[3], "true") == 0 || strcmp(argv[3], "muted") == 0)) {
            muted = 1;
        }
        write_state(kind, value, muted);
    }

    gtk_init(&argc, &argv);

    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_decorated(GTK_WINDOW(window), FALSE);
    gtk_window_set_resizable(GTK_WINDOW(window), FALSE);
    gtk_window_set_keep_above(GTK_WINDOW(window), TRUE);
    gtk_window_set_skip_taskbar_hint(GTK_WINDOW(window), TRUE);
    gtk_window_set_skip_pager_hint(GTK_WINDOW(window), TRUE);
    gtk_window_set_accept_focus(GTK_WINDOW(window), FALSE);
    gtk_window_set_type_hint(GTK_WINDOW(window), GDK_WINDOW_TYPE_HINT_NOTIFICATION);
    gtk_widget_set_name(window, "anto426-osd");
    gtk_style_context_add_class(gtk_widget_get_style_context(window), "osd-root");

    gtk_widget_set_app_paintable(window, TRUE);
    g_signal_connect(window, "draw", G_CALLBACK(on_draw), NULL);

    gtk_widget_set_size_request(window, OSD_WIDTH, OSD_HEIGHT);

    configure_surface(window);
    load_css(window);

    GtkWidget *outer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_name(outer, "osd-box");
    gtk_widget_set_size_request(outer, OSD_WIDTH, OSD_HEIGHT);
    gtk_container_add(GTK_CONTAINER(window), outer);

    icon_label = gtk_label_new("");
    gtk_style_context_add_class(gtk_widget_get_style_context(icon_label), "osd-icon");
    gtk_box_pack_start(GTK_BOX(outer), icon_label, FALSE, FALSE, 0);

    GtkWidget *center_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_name(center_box, "osd-center-box");
    gtk_box_pack_start(GTK_BOX(outer), center_box, TRUE, TRUE, 0);

    title_label = gtk_label_new("");
    gtk_widget_set_halign(title_label, GTK_ALIGN_START);
    gtk_style_context_add_class(gtk_widget_get_style_context(title_label), "osd-title");
    gtk_box_pack_start(GTK_BOX(center_box), title_label, FALSE, FALSE, 0);

    progress_bar = gtk_progress_bar_new();
    gtk_progress_bar_set_show_text(GTK_PROGRESS_BAR(progress_bar), FALSE);
    gtk_widget_set_size_request(progress_bar, BAR_WIDTH, 8);
    gtk_style_context_add_class(gtk_widget_get_style_context(progress_bar), "osd-track");
    gtk_box_pack_start(GTK_BOX(center_box), progress_bar, FALSE, FALSE, 0);

    value_label = gtk_label_new("");
    gtk_widget_set_halign(value_label, GTK_ALIGN_END);
    gtk_label_set_xalign(GTK_LABEL(value_label), 1.0);
    gtk_style_context_add_class(gtk_widget_get_style_context(value_label), "osd-value");
    gtk_box_pack_start(GTK_BOX(outer), value_label, FALSE, FALSE, 0);

    write_pid_file();
    update_osd(window);

    g_unix_signal_add(SIGUSR1, on_sigusr1, window);

    gtk_main();

    cleanup_pid_file();
    return 0;
}
