#include "internal.h"

void anto_launcher_app_entry_free(gpointer data) {
    AppEntry *entry = data;
    if (!entry) return;
    g_clear_object(&entry->info);
    g_free(entry);
}

void anto_launcher_app_record_free(gpointer data) {
    AppRecord *record = data;
    if (!record) return;
    g_free(record->key);
    g_clear_object(&record->info);
    g_free(record->title);
    g_free(record->subtitle);
    g_free(record->icon_key);
    g_free(record);
}

void anto_launcher_app_tile_free(gpointer data) {
    AppTile *tile = data;
    if (!tile) return;
    g_free(tile->icon_key);
    g_free(tile);
}

void anto_launcher_view_free(gpointer data) {
    LauncherView *view = data;
    if (!view) return;
    if (view->scroll_restore_id)
        g_source_remove(view->scroll_restore_id);
    g_clear_pointer(&view->tiles, g_hash_table_unref);
    g_free(view);
}

char *anto_launcher_app_identity(GAppInfo *info) {
    const char *id = g_app_info_get_id(info);
    if (id && *id) return g_strdup(id);

    const char *executable = g_app_info_get_executable(info);
    const char *commandline = g_app_info_get_commandline(info);
    const char *name = g_app_info_get_display_name(info);
    return g_strdup_printf("anonymous:%s\x1f%s\x1f%s",
                           executable ? executable : "",
                           commandline ? commandline : "",
                           name ? name : "");
}

char *anto_launcher_app_description(GAppInfo *info) {
    const char *description = g_app_info_get_description(info);
    if (description && *description) return g_strdup(description);

    const char *executable = g_app_info_get_executable(info);
    if (executable && *executable) return g_strdup(executable);
    return g_strdup("Applicazione desktop");
}

gint anto_launcher_compare_records(gconstpointer left, gconstpointer right) {
    const AppRecord *a = *(AppRecord *const *)left;
    const AppRecord *b = *(AppRecord *const *)right;
    int by_title = g_utf8_collate(a->title, b->title);
    return by_title ? by_title : g_strcmp0(a->key, b->key);
}

GPtrArray *anto_launcher_snapshot(void) {
    GPtrArray *records = g_ptr_array_new_with_free_func(anto_launcher_app_record_free);
    g_autoptr(GHashTable) seen =
        g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
    GList *apps = g_app_info_get_all();

    for (GList *node = apps; node; node = node->next) {
        GAppInfo *info = G_APP_INFO(node->data);
        if (!g_app_info_should_show(info)) continue;

        const char *name = g_app_info_get_display_name(info);
        if (!name || !*name) continue;

        g_autofree char *key = anto_launcher_app_identity(info);
        if (g_hash_table_contains(seen, key)) continue;
        g_hash_table_add(seen, g_strdup(key));

        AppRecord *record = g_new0(AppRecord, 1);
        record->key = g_steal_pointer(&key);
        record->info = g_object_ref(info);
        record->title = g_strdup(name);
        record->subtitle = anto_launcher_app_description(info);
        record->icon_key = anto_launcher_app_icon_key(info);
        g_ptr_array_add(records, record);
    }

    g_list_free_full(apps, g_object_unref);
    g_ptr_array_sort(records, anto_launcher_compare_records);
    return records;
}

GtkWidget *anto_launcher_last_grid_child(GtkWidget *grid) {
    return gtk_widget_get_last_child(grid);
}

AppTile *anto_launcher_capture_app_tile(GtkWidget *child, AppEntry *entry) {
    GtkWidget *card = gtk_flow_box_child_get_child(GTK_FLOW_BOX_CHILD(child));
    GtkWidget *top = card ? gtk_widget_get_first_child(card) : NULL;
    GtkWidget *copy = top ? gtk_widget_get_next_sibling(top) : NULL;
    GtkWidget *icon = top ? gtk_widget_get_first_child(top) : NULL;
    GtkWidget *title = copy ? gtk_widget_get_first_child(copy) : NULL;
    GtkWidget *subtitle = title ? gtk_widget_get_next_sibling(title) : NULL;

    if (!GTK_IS_FLOW_BOX_CHILD(child) || !GTK_IS_IMAGE(icon) ||
        !GTK_IS_LABEL(title))
        return NULL;

    AppTile *tile = g_new0(AppTile, 1);
    tile->child = child;
    tile->icon = icon;
    tile->title = title;
    tile->subtitle = subtitle;
    tile->entry = entry;
    return tile;
}

gboolean anto_launcher_update_app_tile(AppTile *tile, const AppRecord *record) {
    gboolean search_changed = anto_launcher_update_search_metadata(tile, record);
    anto_launcher_set_label_text(tile->title, record->title);
    anto_launcher_set_label_text(tile->subtitle, record->subtitle);

    if (g_strcmp0(tile->icon_key, record->icon_key) != 0) {
        anto_launcher_set_app_icon(tile->icon, record->info);
        g_free(tile->icon_key);
        tile->icon_key = g_strdup(record->icon_key);
    }

    /*
     * The action object is deliberately stable: replacing only its GAppInfo
     * makes an already-mounted tile launch the newest desktop entry without
     * disconnecting signals or replacing the GtkFlowBoxChild.
     */
    g_set_object(&tile->entry->info, record->info);
    return search_changed;
}

gboolean anto_launcher_reconcile_launcher_records(LauncherView *view,
                                           GPtrArray *records) {
    MenuApp *app = view->app;
    g_autoptr(GHashTable) present =
        g_hash_table_new(g_str_hash, g_str_equal);
    gboolean structure_changed = FALSE;
    gboolean filter_changed = FALSE;

    GtkAdjustment *adjustment = gtk_scrolled_window_get_vadjustment(
        GTK_SCROLLED_WINDOW(app->grid_scroll));
    double scroll_value = gtk_adjustment_get_value(adjustment);

    GList *selected =
        gtk_flow_box_get_selected_children(GTK_FLOW_BOX(app->grid));
    GtkWidget *selected_child = selected ? g_object_ref(selected->data) : NULL;
    g_list_free(selected);

    GtkWidget *focused = gtk_window_get_focus(app->window);
    if (focused) g_object_ref(focused);

    for (guint i = 0; i < records->len; i++) {
        AppRecord *record = g_ptr_array_index(records, i);
        g_hash_table_add(present, record->key);
    }

    GHashTableIter iterator;
    gpointer key = NULL;
    gpointer value = NULL;
    g_hash_table_iter_init(&iterator, view->tiles);
    while (g_hash_table_iter_next(&iterator, &key, &value)) {
        if (g_hash_table_contains(present, key)) continue;
        AppTile *tile = value;
        GtkWidget *child = tile->child;
        g_hash_table_iter_remove(&iterator);
        gtk_flow_box_remove(GTK_FLOW_BOX(app->grid), child);
        structure_changed = TRUE;
        filter_changed = TRUE;
    }

    for (guint i = 0; i < records->len; i++) {
        AppRecord *record = g_ptr_array_index(records, i);
        AppTile *tile = g_hash_table_lookup(view->tiles, record->key);
        if (!tile) {
            tile = anto_launcher_add_app_tile(view, record);
            if (!tile) continue;
            g_hash_table_insert(view->tiles, g_strdup(record->key), tile);
            structure_changed = TRUE;
            filter_changed = TRUE;
        } else if (anto_launcher_update_app_tile(tile, record)) {
            filter_changed = TRUE;
        }

        int desired = (int)i;
        int current = gtk_flow_box_child_get_index(
            GTK_FLOW_BOX_CHILD(tile->child));
        if (current != desired) {
            g_object_ref(tile->child);
            gtk_flow_box_remove(GTK_FLOW_BOX(app->grid), tile->child);
            gtk_flow_box_insert(GTK_FLOW_BOX(app->grid), tile->child,
                                desired);
            g_object_unref(tile->child);
            structure_changed = TRUE;
        }
    }

    if (filter_changed)
        gtk_flow_box_invalidate_filter(GTK_FLOW_BOX(app->grid));

    if (selected_child &&
        gtk_widget_get_parent(selected_child) == app->grid)
        gtk_flow_box_select_child(GTK_FLOW_BOX(app->grid),
                                  GTK_FLOW_BOX_CHILD(selected_child));

    if (focused && gtk_window_get_focus(app->window) != focused &&
        gtk_widget_get_root(focused) == GTK_ROOT(app->window))
        gtk_widget_grab_focus(focused);

    g_clear_object(&selected_child);
    g_clear_object(&focused);

    if (structure_changed)
        anto_launcher_schedule_scroll_restore(view, scroll_value);
    return structure_changed || filter_changed;
}

gboolean anto_launcher_reconcile_launcher(LauncherView *view) {
    g_autoptr(GPtrArray) records = anto_launcher_snapshot();
    return anto_launcher_reconcile_launcher_records(view, records);
}

void menu_launcher_live_event(MenuApp *app) {
    if (!app || !app->window ||
        g_strcmp0(app->current_page, "apps") != 0)
        return;

    LauncherView *view =
        g_object_get_data(G_OBJECT(app->window), LAUNCHER_VIEW_KEY);
    if (!view) return;
    anto_launcher_reconcile_launcher(view);
}
