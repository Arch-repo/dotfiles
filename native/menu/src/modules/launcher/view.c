#include "internal.h"

const char *anto_launcher_app_icon_name(GAppInfo *info) {
    GIcon *icon = g_app_info_get_icon(info);
    if (!icon) return "application-x-executable-symbolic";
    if (G_IS_THEMED_ICON(icon)) {
        const char *const *names = g_themed_icon_get_names(G_THEMED_ICON(icon));
        if (names && names[0]) return names[0];
    }
    return "application-x-executable-symbolic";
}

void anto_launcher_set_app_icon(GtkWidget *image, GAppInfo *info) {
    GIcon *icon = info ? g_app_info_get_icon(info) : NULL;
    if (icon && G_IS_FILE_ICON(icon)) {
        GFile *file = g_file_icon_get_file(G_FILE_ICON(icon));
        if (!file || !g_file_query_exists(file, NULL))
            icon = NULL;
    }
    if (icon)
        gtk_image_set_from_gicon(GTK_IMAGE(image), icon);
    else
        gtk_image_set_from_icon_name(
            GTK_IMAGE(image), "application-x-executable-symbolic");
}

void anto_launcher_set_label_text(GtkWidget *label, const char *text) {
    if (!GTK_IS_LABEL(label)) return;
    const char *current = gtk_label_get_text(GTK_LABEL(label));
    if (g_strcmp0(current, text ? text : "") != 0)
        gtk_label_set_text(GTK_LABEL(label), text ? text : "");
}

gboolean anto_launcher_update_search_metadata(AppTile *tile,
                                       const AppRecord *record) {
    const char *executable = g_app_info_get_executable(record->info);
    const char *id = g_app_info_get_id(record->info);
    g_autofree char *combined = g_strdup_printf(
        "%s %s %s %s", record->title, record->subtitle,
        executable ? executable : "", id ? id : "");
    g_autofree char *search = g_utf8_strdown(combined, -1);
    const char *current =
        g_object_get_data(G_OBJECT(tile->child), "menu-search");
    if (g_strcmp0(current, search) == 0) return FALSE;
    g_object_set_data_full(G_OBJECT(tile->child), "menu-search",
                           g_strdup(search), g_free);
    return TRUE;
}

AppTile *anto_launcher_add_app_tile(LauncherView *view,
                             const AppRecord *record) {
    MenuApp *app = view->app;
    GtkWidget *before = anto_launcher_last_grid_child(app->grid);
    AppEntry *entry = g_new0(AppEntry, 1);
    entry->info = g_object_ref(record->info);
    menu_add_tile(app, anto_launcher_app_icon_name(record->info), record->title,
                  NULL, NULL, anto_launcher_launch_app, entry, anto_launcher_app_entry_free);

    GtkWidget *child = anto_launcher_last_grid_child(app->grid);
    if (!child || child == before) return NULL;
    gtk_widget_add_css_class(child, "launcher-tile");
    gtk_widget_set_tooltip_text(child, record->title);

    AppTile *tile = anto_launcher_capture_app_tile(child, entry);
    if (!tile) {
        gtk_flow_box_remove(GTK_FLOW_BOX(app->grid), child);
        return NULL;
    }
    g_object_set_data_full(G_OBJECT(child), "launcher-app-key",
                           g_strdup(record->key), g_free);
    anto_launcher_update_app_tile(tile, record);
    return tile;
}

gboolean anto_launcher_restore_launcher_scroll(gpointer data) {
    LauncherView *view = data;
    view->scroll_restore_id = 0;
    if (!view->app || !view->app->window ||
        g_object_get_data(G_OBJECT(view->app->window), LAUNCHER_VIEW_KEY) != view ||
        g_strcmp0(view->app->current_page, "apps") != 0)
        return G_SOURCE_REMOVE;

    GtkAdjustment *adjustment = gtk_scrolled_window_get_vadjustment(
        GTK_SCROLLED_WINDOW(view->app->grid_scroll));
    double lower = gtk_adjustment_get_lower(adjustment);
    double maximum = MAX(lower,
                         gtk_adjustment_get_upper(adjustment) -
                             gtk_adjustment_get_page_size(adjustment));
    gtk_adjustment_set_value(
        adjustment, CLAMP(view->scroll_value, lower, maximum));
    return G_SOURCE_REMOVE;
}

void anto_launcher_schedule_scroll_restore(LauncherView *view, double value) {
    view->scroll_value = value;
    if (view->scroll_restore_id) return;
    view->scroll_restore_id =
        g_idle_add_full(G_PRIORITY_DEFAULT_IDLE, anto_launcher_restore_launcher_scroll,
                        view, NULL);
}

void menu_show_launcher(MenuApp *app) {
    /*
     * This is the only launcher entry point allowed to reconstruct the page:
     * it is called by explicit navigation.  Live app-info events below only
     * reconcile the children of this mounted view.
     */
    g_object_set_data(G_OBJECT(app->window), LAUNCHER_VIEW_KEY, NULL);
    menu_page_begin(app, "view-app-grid-symbolic", "Applicazioni",
                    "Le applicazioni installate, ordinate e sempre aggiornate",
                    "Cerca tra le applicazioni…");
    menu_set_layout(app, MENU_LAYOUT_GRID);
    /* Application cards intentionally share the exact 3-column geometry of
     * the action dashboards.  Their content stays simpler (icon + name), but
     * width, height, padding and icon scale come from the common tile rules. */
    menu_set_grid_columns(app, 3);

    LauncherView *view = g_new0(LauncherView, 1);
    view->app = app;
    view->tiles =
        g_hash_table_new_full(g_str_hash, g_str_equal, g_free, anto_launcher_app_tile_free);
    g_object_set_data_full(G_OBJECT(app->window), LAUNCHER_VIEW_KEY, view,
                           anto_launcher_view_free);
    anto_launcher_reconcile_launcher(view);
    menu_set_footer(app, "←↑↓→ naviga  ·  Invio avvia  ·  digita per filtrare");
}
