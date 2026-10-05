#include "palette.h"

static GtkCssProvider *provider;
static GFileMonitor *monitor;
static GdkDisplay *palette_display;
static char *palette_path;
static guint debounce;

static void invalid_css(GtkCssProvider *style, GtkCssSection *section,
                        const GError *error, gpointer data) {
    (void)style; (void)section; (void)error;
    *(gboolean *)data = TRUE;
}

static gboolean complete_palette(const char *css) {
    const char *roles[] = {"background", "surface", "base", "base-alt", "foreground",
                          "accent", "selected-fg", "border", "muted", "red", "yellow", "green"};
    for (guint i = 0; i < G_N_ELEMENTS(roles); i++) {
        g_autofree char *pattern = g_strdup_printf(
            "(?m)^\\s*@define-color\\s+%s\\s+#[[:xdigit:]]{6}\\s*;", roles[i]);
        if (!g_regex_match_simple(pattern, css, 0, 0)) return FALSE;
    }
    return TRUE;
}

static void redraw_custom_content(GtkWidget *widget) {
    if (GTK_IS_DRAWING_AREA(widget)) gtk_widget_queue_draw(widget);
    for (GtkWidget *child = gtk_widget_get_first_child(widget); child;
         child = gtk_widget_get_next_sibling(child)) redraw_custom_content(child);
}

static gboolean reload(gpointer data) {
    (void)data;
    debounce = 0;
    g_autofree char *css = NULL;
    gsize length = 0;
    if (!g_file_get_contents(palette_path, &css, &length, NULL) || length > 256 * 1024 ||
        !complete_palette(css)) return G_SOURCE_REMOVE;

    GtkCssProvider *next = gtk_css_provider_new();
    gboolean invalid = FALSE;
    gulong check = g_signal_connect(next, "parsing-error", G_CALLBACK(invalid_css), &invalid);
    gtk_css_provider_load_from_string(next, css);
    g_signal_handler_disconnect(next, check);
    if (invalid) {
        g_object_unref(next);
        return G_SOURCE_REMOVE;
    }
    /* Validate a complete generation before replacing the visible palette. */
    gtk_style_context_add_provider_for_display(palette_display, GTK_STYLE_PROVIDER(next),
                                               GTK_STYLE_PROVIDER_PRIORITY_USER + 10);
    if (provider) {
        gtk_style_context_remove_provider_for_display(palette_display, GTK_STYLE_PROVIDER(provider));
        g_object_unref(provider);
    }
    provider = next;
    GListModel *windows = gtk_window_get_toplevels();
    for (guint i = 0; i < g_list_model_get_n_items(windows); i++) {
        g_autoptr(GtkWindow) window = g_list_model_get_item(windows, i);
        redraw_custom_content(GTK_WIDGET(window));
    }
    return G_SOURCE_REMOVE;
}

static void changed(GFileMonitor *watch, GFile *file, GFile *other,
                    GFileMonitorEvent event, gpointer data) {
    (void)watch; (void)event; (void)data;
    g_autofree char *name = file ? g_file_get_basename(file) : NULL;
    g_autofree char *other_name = other ? g_file_get_basename(other) : NULL;
    if (g_strcmp0(name, "colors.css") && g_strcmp0(other_name, "colors.css")) return;
    if (!debounce) debounce = g_timeout_add(80, reload, NULL);
}

void anto_watch_palette(GdkDisplay *display) {
    if (palette_display || !display) return;
    palette_display = display;
    palette_path = g_build_filename(g_get_user_config_dir(), "anto426-local", "theme", "colors.css", NULL);
    reload(NULL);
    g_autofree char *directory = g_path_get_dirname(palette_path);
    g_mkdir_with_parents(directory, 0700);
    g_autoptr(GFile) folder = g_file_new_for_path(directory);
    monitor = g_file_monitor_directory(folder, G_FILE_MONITOR_WATCH_MOVES, NULL, NULL);
    if (monitor) g_signal_connect(monitor, "changed", G_CALLBACK(changed), NULL);
}
