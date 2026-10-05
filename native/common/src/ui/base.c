#include "primitives.h"
#include "palette.h"

void anto_ui_init(GdkDisplay *display) {
    if (!display || g_object_get_data(G_OBJECT(display), "anto-design-loaded")) return;
    const char *names[] = {"tokens.css", "design.css", "primitives.css"};
    for (guint i = 0; i < G_N_ELEMENTS(names); i++) {
        g_autofree char *path = g_build_filename(g_get_home_dir(), ".local/share/anto-desktop", names[i], NULL);
        GtkCssProvider *style = gtk_css_provider_new();
        gtk_css_provider_load_from_path(style, path);
        gtk_style_context_add_provider_for_display(display, GTK_STYLE_PROVIDER(style), GTK_STYLE_PROVIDER_PRIORITY_USER + 1 + i);
        g_object_unref(style);
    }
    anto_watch_palette(display);
    g_object_set_data(G_OBJECT(display), "anto-design-loaded", GINT_TO_POINTER(1));
}
