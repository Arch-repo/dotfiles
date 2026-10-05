#include "internal.h"
#include "primitives.h"

gboolean anto_system_tile_search_set(GtkWidget *tile, const char *title, const char *subtitle, const char *value) {
    if (!tile) return FALSE;
    g_autofree char *text = g_strdup_printf("%s %s %s", title ? title : "", subtitle ? subtitle : "", value ? value : "");
    g_autofree char *normalized = g_utf8_strdown(text, -1);
    if (g_strcmp0(g_object_get_data(G_OBJECT(tile), "menu-search"), normalized) == 0) return FALSE;
    g_object_set_data_full(G_OBJECT(tile), "menu-search", g_steal_pointer(&normalized), g_free);
    return TRUE;
}
void anto_system_render(SystemLive *live, const SystemSnapshot *snapshot) {
    const SystemSnapshot empty = {0};
    if (!snapshot) snapshot = &empty;
    MenuApp *app = live->app;
    menu_page_begin(app, "preferences-system-symbolic", "Panoramica", "Il desktop, a colpo d’occhio", "Cerca tra i controlli…");
    menu_set_layout(app, MENU_LAYOUT_GRID);
    menu_set_grid_columns(app, 2);
    gtk_widget_add_css_class(app->grid, "overview-grid");
    struct { const char *page, *icon, *title, *detail, *value; } controls[] = {
        {"audio", "audio-speakers-symbolic", "Audio", "Volume, microfono e player", snapshot->volume},
        {"wifi", "network-wireless-symbolic", "Wi-Fi", "Reti e profili salvati", snapshot->network},
        {"bluetooth", "bluetooth-symbolic", "Bluetooth", "Cuffie e dispositivi", snapshot->bluetooth},
        {"brightness", "display-brightness-symbolic", "Energia", "Luminosità e profilo", snapshot->brightness},
        {"display", "video-display-symbolic", "Schermi", "Layout, profili e mirroring", snapshot->displays},
    };
    for (guint i = 0; i < G_N_ELEMENTS(controls); i++) {
        menu_add_nav_tile(app, controls[i].icon, controls[i].title, controls[i].detail,
                          anto_system_snapshot_text(controls[i].value, "In aggiornamento…"), controls[i].page);
        live->tiles[i] = anto_system_tile_at(app, i);
        live->badges[i] = anto_system_find_widget_with_class(live->tiles[i], "tile-badge");
    }
    menu_add_nav_tile(app, "preferences-desktop-wallpaper-symbolic", "Personalizza", "Sfondi e colori del desktop", "La tua raccolta", "wallpaper");
    menu_set_footer(app, "Scegli una sezione nella barra laterale · Esc chiude");
    live->mounted = TRUE;
}
void menu_show_system(MenuApp *app) {
    SystemLive *live = anto_system_live_get(app);
    live->mounted = FALSE;
    memset(live->badges, 0, sizeof(live->badges));
    memset(live->tiles, 0, sizeof(live->tiles));
    if (app->desktop_snapshot) menu_system_accept(app, app->desktop_snapshot);
    anto_system_render(live, live->snapshot);
    if (!app->desktop_snapshot && !anto_query_busy(app->summary))
        anto_system_refresh_start(live);
}
