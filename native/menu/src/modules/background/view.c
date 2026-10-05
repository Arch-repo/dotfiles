#include "internal.h"

gboolean anto_background_view_is_current(BackgroundRuntime *runtime) {
    GtkWidget *root = g_weak_ref_get(&runtime->root);
    gboolean current =
        root && g_strcmp0(runtime->app->current_page, "background") == 0 &&
        gtk_widget_get_parent(root) == runtime->app->list;
    g_clear_object(&root);
    return current;
}

void anto_background_update_search_data(GtkWidget *row, const char *title,
                               const char *subtitle) {
    g_autofree char *combined =
        g_strdup_printf("%s %s", title ? title : "",
                        subtitle ? subtitle : "");
    g_autofree char *search = g_utf8_strdown(combined, -1);
    g_object_set_data_full(G_OBJECT(row), "menu-search",
                           g_strdup(search), g_free);
}

BackgroundRow *anto_background_add_window(
    BackgroundRuntime *runtime, const BackgroundWindow *window) {
    WindowEntry *entry = g_new0(WindowEntry, 1);
    entry->address = g_strdup(window->address);
    g_autofree char *detail = anto_background_window_detail(window);
    menu_add_item(runtime->app, "focus-windows-symbolic",
                  window->title, detail, NULL,
                  anto_background_focus_window, entry, anto_background_window_entry_free);

    GtkWidget *row =
        gtk_widget_get_last_child(runtime->app->list);
    BackgroundRow *binding = g_new0(BackgroundRow, 1);
    binding->row = row;
    binding->title = anto_background_find_css_descendant(row, "item-title");
    binding->subtitle =
        anto_background_find_css_descendant(row, "item-subtitle");
    g_hash_table_insert(runtime->rows, g_strdup(window->address),
                        binding);
    return binding;
}

void anto_background_add_empty(BackgroundRuntime *runtime) {
    if (runtime->empty_row) return;
    menu_add_item(runtime->app, "focus-windows-symbolic",
                  "Nessuna finestra elencabile",
                  "La sessione non espone client attivi",
                  NULL, NULL, NULL, NULL);
    runtime->empty_row =
        gtk_widget_get_last_child(runtime->app->list);
}

gboolean anto_background_restore_scroll(gpointer data) {
    BackgroundRuntime *runtime = data;
    runtime->scroll_restore_id = 0;
    if (!anto_background_view_is_current(runtime)) return G_SOURCE_REMOVE;
    GtkAdjustment *adjustment =
        gtk_scrolled_window_get_vadjustment(
            GTK_SCROLLED_WINDOW(runtime->app->list_scroll));
    double lower = gtk_adjustment_get_lower(adjustment);
    double maximum =
        MAX(lower, gtk_adjustment_get_upper(adjustment) -
                       gtk_adjustment_get_page_size(adjustment));
    gtk_adjustment_set_value(
        adjustment, CLAMP(runtime->scroll_value, lower, maximum));
    return G_SOURCE_REMOVE;
}

void anto_background_schedule_scroll(BackgroundRuntime *runtime,
                                       double value) {
    runtime->scroll_value = value;
    if (!runtime->scroll_restore_id)
        runtime->scroll_restore_id = g_idle_add_full(
            G_PRIORITY_DEFAULT_IDLE, anto_background_restore_scroll,
            runtime, NULL);
}

void menu_show_background(MenuApp *app) {
    BackgroundRuntime *runtime = anto_background_runtime_get(app);
    g_hash_table_remove_all(runtime->rows);
    runtime->empty_row = NULL;
    g_weak_ref_set(&runtime->root, NULL);

    const BackgroundSnapshot *snapshot = runtime->snapshot;
    guint count = snapshot && snapshot->valid && snapshot->windows
                      ? snapshot->windows->len
                      : 0;
    g_autofree char *subtitle = snapshot && snapshot->valid
        ? g_strdup_printf(
              "Sessione utente · %s · %s · %u %s",
              snapshot->cpu && *snapshot->cpu
                  ? snapshot->cpu
                  : "CPU n/d",
              snapshot->memory && *snapshot->memory
                  ? snapshot->memory
                  : "RAM n/d",
              count, count == 1 ? "finestra" : "finestre")
        : g_strdup("Sessione utente · lettura in corso…");
    menu_page_begin(app, "system-run-symbolic", "App in background",
                    subtitle, "Cerca una finestra…");
    menu_add_shell_item(app, "utilities-system-monitor-symbolic",
                        "Apri Btop",
                        "Vista completa di processi e risorse", NULL,
                        "ghostty -e btop", TRUE);
    GtkWidget *root = gtk_widget_get_last_child(app->list);
    if (root)
        g_weak_ref_set(&runtime->root, G_OBJECT(root));
    menu_add_section(app, "FINESTRE APERTE");
    GtkWidget *section = gtk_widget_get_last_child(app->list);
    runtime->rows_start_index = section
        ? gtk_list_box_row_get_index(GTK_LIST_BOX_ROW(section)) + 1
        : 0;

    if (snapshot && snapshot->valid) {
        for (guint index = 0; index < count; index++) {
            BackgroundWindow *window =
                g_ptr_array_index(snapshot->windows, index);
            anto_background_add_window(runtime, window);
        }
        if (count == 0) anto_background_add_empty(runtime);
    } else {
        anto_background_add_empty(runtime);
    }

    menu_set_footer(
        app,
        "Titolo, workspace e metriche cambiano in-place · Invio porta in primo piano");
    anto_background_refresh_start(runtime);
}
