#include "internal.h"


void anto_background_window_entry_free(gpointer data) {
    WindowEntry *entry = data;
    if (!entry) return;
    g_free(entry->address);
    g_free(entry);
}

void anto_background_window_free(gpointer data) {
    BackgroundWindow *window = data;
    if (!window) return;
    g_free(window->address);
    g_free(window->title);
    g_free(window->class_name);
    g_free(window);
}

void anto_background_snapshot_free(BackgroundSnapshot *snapshot) {
    if (!snapshot) return;
    g_free(snapshot->cpu);
    g_free(snapshot->memory);
    if (snapshot->windows) g_ptr_array_free(snapshot->windows, TRUE);
    g_free(snapshot);
}

void anto_background_row_free(gpointer data) {
    g_free(data);
}

void anto_background_runtime_free(gpointer data) {
    BackgroundRuntime *runtime = data;
    if (!runtime) return;
    if (runtime->scroll_restore_id)
        g_source_remove(runtime->scroll_restore_id);
    anto_background_snapshot_free(runtime->snapshot);
    g_hash_table_destroy(runtime->rows);
    g_weak_ref_clear(&runtime->root);
    anto_query_close(runtime->query);
    g_clear_object(&runtime->query);
    g_free(runtime);
}

BackgroundRuntime *anto_background_runtime_get(MenuApp *app) {
    BackgroundRuntime *runtime =
        g_object_get_data(G_OBJECT(app->window), BACKGROUND_RUNTIME_KEY);
    if (runtime) return runtime;
    runtime = g_new0(BackgroundRuntime, 1);
    runtime->app = app;
    runtime->rows = g_hash_table_new_full(
        g_str_hash, g_str_equal, g_free, anto_background_row_free);
    g_weak_ref_init(&runtime->root, NULL);
    g_object_set_data_full(G_OBJECT(app->window), BACKGROUND_RUNTIME_KEY,
                           runtime, anto_background_runtime_free);
    return runtime;
}

const char *anto_background_json_string(struct json_object *object, const char *key) {
    struct json_object *value = NULL;
    return json_object_object_get_ex(object, key, &value)
               ? json_object_get_string(value)
               : "";
}

char *anto_background_metric_from_header(const char *header, const char *key) {
    g_auto(GStrv) lines = g_strsplit(header ? header : "", "\n", -1);
    for (guint index = 0; lines[index]; index++) {
        g_auto(GStrv) fields = g_strsplit(lines[index], "\t", 2);
        if (fields[0] && fields[1] &&
            g_strcmp0(fields[0], key) == 0) {
            g_strstrip(fields[1]);
            return g_strdup(fields[1]);
        }
    }
    return NULL;
}

BackgroundSnapshot *anto_background_snapshot_parse(const char *output) {
    BackgroundSnapshot *snapshot = g_new0(BackgroundSnapshot, 1);
    snapshot->windows =
        g_ptr_array_new_with_free_func(anto_background_window_free);

    const char marker[] = "\nCLIENTS\n";
    const char *split = output ? strstr(output, marker) : NULL;
    if (!split) return snapshot;
    g_autofree char *header =
        g_strndup(output, (gsize)(split - output));
    snapshot->cpu = anto_background_metric_from_header(header, "CPU");
    snapshot->memory = anto_background_metric_from_header(header, "MEMORY");

    struct json_object *root =
        json_tokener_parse(split + strlen(marker));
    if (!root || !json_object_is_type(root, json_type_array)) {
        if (root) json_object_put(root);
        return snapshot;
    }

    size_t count = json_object_array_length(root);
    for (size_t index = 0; index < count; index++) {
        struct json_object *client =
            json_object_array_get_idx(root, index);
        const char *address = anto_background_json_string(client, "address");
        const char *title = anto_background_json_string(client, "title");
        const char *class_name = anto_background_json_string(client, "class");
        if (!*address || !*title ||
            g_str_has_prefix(class_name, "anto426.widget."))
            continue;

        BackgroundWindow *window = g_new0(BackgroundWindow, 1);
        window->address = g_strdup(address);
        window->title = g_strdup(title);
        window->class_name = g_strdup(
            *class_name ? class_name : "app");
        struct json_object *workspace = NULL;
        struct json_object *workspace_id = NULL;
        if (json_object_object_get_ex(client, "workspace", &workspace) &&
            json_object_is_type(workspace, json_type_object) &&
            json_object_object_get_ex(workspace, "id", &workspace_id))
            window->workspace = json_object_get_int(workspace_id);
        g_ptr_array_add(snapshot->windows, window);
    }

    snapshot->valid = TRUE;
    json_object_put(root);
    return snapshot;
}

void anto_background_focus_window(MenuApp *app, gpointer data) {
    WindowEntry *entry = data;
    g_autofree char *selector =
        g_strdup_printf("address:%s", entry->address);
    const char *argv[] = {
        "hyprctl", "dispatch", "focuswindow", selector, NULL,
    };
    menu_spawn(app, argv, TRUE);
}

GtkWidget *anto_background_find_css_descendant(GtkWidget *widget,
                                      const char *css_class) {
    if (!widget) return NULL;
    if (gtk_widget_has_css_class(widget, css_class)) return widget;
    for (GtkWidget *child = gtk_widget_get_first_child(widget);
         child; child = gtk_widget_get_next_sibling(child)) {
        GtkWidget *match = anto_background_find_css_descendant(child, css_class);
        if (match) return match;
    }
    return NULL;
}

char *anto_background_window_detail(const BackgroundWindow *window) {
    return g_strdup_printf("%s · workspace %d",
                           window->class_name && *window->class_name
                               ? window->class_name
                               : "app",
                           window->workspace);
}

void anto_background_update_window(BackgroundRow *binding,
                                     const BackgroundWindow *window) {
    g_autofree char *detail = anto_background_window_detail(window);
    gboolean changed = FALSE;
    if (GTK_IS_LABEL(binding->title) &&
        g_strcmp0(gtk_label_get_text(GTK_LABEL(binding->title)),
                  window->title) != 0) {
        gtk_label_set_text(GTK_LABEL(binding->title), window->title);
        changed = TRUE;
    }
    if (GTK_IS_LABEL(binding->subtitle) &&
        g_strcmp0(gtk_label_get_text(GTK_LABEL(binding->subtitle)),
                  detail) != 0) {
        gtk_label_set_text(GTK_LABEL(binding->subtitle), detail);
        changed = TRUE;
    }
    if (changed)
        anto_background_update_search_data(binding->row, window->title, detail);
}

void anto_background_remove_empty(BackgroundRuntime *runtime) {
    if (!runtime->empty_row) return;
    GtkWidget *row = runtime->empty_row;
    runtime->empty_row = NULL;
    if (gtk_widget_get_parent(row) == runtime->app->list)
        gtk_list_box_remove(GTK_LIST_BOX(runtime->app->list), row);
}

void anto_background_apply_snapshot(BackgroundRuntime *runtime,
                                      const BackgroundSnapshot *snapshot) {
    if (!anto_background_view_is_current(runtime) ||
        !snapshot || !snapshot->valid)
        return;

    guint count = snapshot->windows ? snapshot->windows->len : 0;
    g_autofree char *subtitle = g_strdup_printf(
        "Sessione utente · %s · %s · %u %s",
        snapshot->cpu && *snapshot->cpu ? snapshot->cpu : "CPU n/d",
        snapshot->memory && *snapshot->memory
            ? snapshot->memory
            : "RAM n/d",
        count, count == 1 ? "finestra" : "finestre");
    gtk_label_set_text(GTK_LABEL(runtime->app->page_subtitle),
                       subtitle);
    GtkAdjustment *adjustment =
        gtk_scrolled_window_get_vadjustment(
            GTK_SCROLLED_WINDOW(runtime->app->list_scroll));
    double scroll = gtk_adjustment_get_value(adjustment);
    gboolean structure_changed = FALSE;

    g_autoptr(GHashTable) present =
        g_hash_table_new(g_str_hash, g_str_equal);
    for (guint index = 0; index < count; index++) {
        BackgroundWindow *window =
            g_ptr_array_index(snapshot->windows, index);
        g_hash_table_add(present, window->address);
        BackgroundRow *binding =
            g_hash_table_lookup(runtime->rows, window->address);
        if (!binding) {
            if (runtime->empty_row) structure_changed = TRUE;
            anto_background_remove_empty(runtime);
            binding = anto_background_add_window(runtime, window);
            structure_changed = TRUE;
        }
        anto_background_update_window(binding, window);
    }

    GHashTableIter iterator;
    gpointer key = NULL;
    gpointer value = NULL;
    g_hash_table_iter_init(&iterator, runtime->rows);
    while (g_hash_table_iter_next(&iterator, &key, &value)) {
        if (g_hash_table_contains(present, key)) continue;
        BackgroundRow *binding = value;
        GtkWidget *row = binding->row;
        if (gtk_widget_get_parent(row) == runtime->app->list)
            gtk_list_box_remove(GTK_LIST_BOX(runtime->app->list), row);
        g_hash_table_iter_remove(&iterator);
        structure_changed = TRUE;
    }

    if (count == 0) {
        if (!runtime->empty_row) structure_changed = TRUE;
        anto_background_add_empty(runtime);
    } else {
        if (runtime->empty_row) structure_changed = TRUE;
        anto_background_remove_empty(runtime);
    }

    for (guint index = 0; index < count; index++) {
        BackgroundWindow *window =
            g_ptr_array_index(snapshot->windows, index);
        BackgroundRow *binding =
            g_hash_table_lookup(runtime->rows, window->address);
        if (!binding) continue;
        int target = runtime->rows_start_index + (int)index;
        int current = gtk_list_box_row_get_index(
            GTK_LIST_BOX_ROW(binding->row));
        if (current == target) continue;
        g_object_ref(binding->row);
        gtk_list_box_remove(GTK_LIST_BOX(runtime->app->list),
                            binding->row);
        gtk_list_box_insert(GTK_LIST_BOX(runtime->app->list),
                            binding->row, target);
        g_object_unref(binding->row);
        structure_changed = TRUE;
    }

    /* Invalidation only re-evaluates the existing rows against the current
     * query; it does not replace the list, selection or adjustment. */
    gtk_list_box_invalidate_filter(GTK_LIST_BOX(runtime->app->list));
    if (structure_changed)
        anto_background_schedule_scroll(runtime, scroll);
}

static void background_received(const char *output, const GError *error, gboolean changed, gpointer data) {
    (void)changed;
    BackgroundRuntime *runtime = data;
    if (runtime->app->closing) return;
    BackgroundSnapshot *snapshot =
        output && !error ? anto_background_snapshot_parse(output) : NULL;
    if (snapshot && snapshot->valid) {
        anto_background_snapshot_free(runtime->snapshot);
        runtime->snapshot = snapshot;
        anto_background_apply_snapshot(runtime, runtime->snapshot);
    } else {
        anto_background_snapshot_free(snapshot);
    }

}


void anto_background_refresh_start(BackgroundRuntime *runtime) {
    if (!runtime->query) {
        g_autofree char *backend = menu_backend_path();
        const char *argv[] = {backend, "background", "snapshot", NULL};
        const char *override = g_getenv("ANTO_MENU_BACKGROUND_SNAPSHOT_CMD");
        const char *test_argv[] = {"/bin/sh", "-lc", override, NULL};
        runtime->query = anto_query_new(G_OBJECT(runtime->app->window), override && *override ? test_argv : argv, 5, background_received, runtime);
    }
    anto_query_request(runtime->query);
}


void menu_background_live_event(MenuApp *app) {
    if (!app || !app->window ||
        g_strcmp0(app->current_page, "background") != 0)
        return;
    anto_background_refresh_start(anto_background_runtime_get(app));
}
