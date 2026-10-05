#include "menu.h"
#include "ui_internal.h"
#include "local_config.h"
#include "palette.h"
#include "glass.h"
#include "primitives.h"
#include <gtk4-layer-shell.h>
#include <json-c/json.h>
#include <stdlib.h>
#include <string.h>
void load_css(void) {
    GdkDisplay *display = gdk_display_get_default();
    if (!display) return;

    g_autofree char *style = menu_home_path(".local/share/anto-desktop/menu.css");
    g_autofree char *wallpaper = menu_home_path(".local/share/anto-desktop/wallpaper.css");
    anto_ui_init(display);
    const char *paths[] = {style, wallpaper};
    for (guint i = 0; i < G_N_ELEMENTS(paths); i++) {
        if (!g_file_test(paths[i], G_FILE_TEST_IS_REGULAR)) continue;
        GtkCssProvider *provider = gtk_css_provider_new();
        gtk_css_provider_load_from_path(provider, paths[i]);
        gtk_style_context_add_provider_for_display(
            display, GTK_STYLE_PROVIDER(provider),
            GTK_STYLE_PROVIDER_PRIORITY_USER + 3);
        g_object_unref(provider);
    }
    anto_load_glass_style(display);
}
