#include "internal.h"

void anto_floating_snapshot_free(FloatingSnapshot *snapshot) {
    if (!snapshot) return;
    g_free(snapshot->address);
    g_free(snapshot->class_name);
    g_free(snapshot->title);
    g_free(snapshot);
}

const char *anto_floating_json_string(struct json_object *object, const char *key) {
    struct json_object *value = NULL;
    return json_object_object_get_ex(object, key, &value)
               ? json_object_get_string(value)
               : "";
}

gboolean anto_floating_json_boolean(struct json_object *object, const char *key) {
    struct json_object *value = NULL;
    return json_object_object_get_ex(object, key, &value) &&
           json_object_get_boolean(value);
}

FloatingSnapshot *anto_floating_snapshot_parse(const char *output) {
    FloatingSnapshot *snapshot = g_new0(FloatingSnapshot, 1);
    struct json_object *root = json_tokener_parse(output ? output : "");
    if (!root || !json_object_is_type(root, json_type_object)) {
        if (root) json_object_put(root);
        return snapshot;
    }

    snapshot->address = g_strdup(anto_floating_json_string(root, "address"));
    snapshot->class_name = g_strdup(anto_floating_json_string(root, "class"));
    snapshot->title = g_strdup(anto_floating_json_string(root, "title"));
    snapshot->floating = anto_floating_json_boolean(root, "floating");
    snapshot->pinned = anto_floating_json_boolean(root, "pinned");
    snapshot->fullscreen = anto_floating_json_boolean(root, "fullscreen");

    struct json_object *workspace = NULL;
    struct json_object *workspace_id = NULL;
    if (json_object_object_get_ex(root, "workspace", &workspace) &&
        json_object_is_type(workspace, json_type_object) &&
        json_object_object_get_ex(workspace, "id", &workspace_id))
        snapshot->workspace = json_object_get_int(workspace_id);

    snapshot->valid = snapshot->address && *snapshot->address;
    json_object_put(root);
    return snapshot;
}

void anto_floating_runtime_free(gpointer data) {
    FloatingRuntime *runtime = data;
    if (!runtime) return;
    anto_floating_snapshot_free(runtime->snapshot);
    g_weak_ref_clear(&runtime->root);
    anto_query_close(runtime->query);
    g_clear_object(&runtime->query);
    g_free(runtime);
}

FloatingRuntime *anto_floating_runtime_get(MenuApp *app) {
    FloatingRuntime *runtime =
        g_object_get_data(G_OBJECT(app->window), FLOATING_RUNTIME_KEY);
    if (runtime) return runtime;
    runtime = g_new0(FloatingRuntime, 1);
    runtime->app = app;
    g_weak_ref_init(&runtime->root, NULL);
    g_object_set_data_full(G_OBJECT(app->window), FLOATING_RUNTIME_KEY,
                           runtime, anto_floating_runtime_free);
    return runtime;
}

GtkWidget *anto_floating_find_css_descendant(GtkWidget *widget,
                                      const char *css_class) {
    if (!widget) return NULL;
    if (gtk_widget_has_css_class(widget, css_class)) return widget;
    for (GtkWidget *child = gtk_widget_get_first_child(widget);
         child; child = gtk_widget_get_next_sibling(child)) {
        GtkWidget *match = anto_floating_find_css_descendant(child, css_class);
        if (match) return match;
    }
    return NULL;
}

GtkWidget *anto_floating_tile_badge_at(MenuApp *app, int index) {
    GtkFlowBoxChild *child =
        gtk_flow_box_get_child_at_index(GTK_FLOW_BOX(app->grid), index);
    return child
               ? anto_floating_find_css_descendant(GTK_WIDGET(child), "tile-badge")
               : NULL;
}

char *anto_floating_subtitle(const FloatingSnapshot *snapshot) {
    if (!snapshot || !snapshot->valid)
        return g_strdup("Nessuna finestra attiva");
    const char *class_name =
        snapshot->class_name && *snapshot->class_name
            ? snapshot->class_name
            : "Finestra";
    const char *title =
        snapshot->title && *snapshot->title ? snapshot->title : "Senza titolo";
    if (snapshot->workspace > 0)
        return g_strdup_printf("%s · %s · workspace %d%s",
                               class_name, title, snapshot->workspace,
                               snapshot->fullscreen ? " · fullscreen" : "");
    return g_strdup_printf("%s · %s%s", class_name, title,
                           snapshot->fullscreen ? " · fullscreen" : "");
}

void anto_floating_apply_snapshot(FloatingRuntime *runtime,
                                    const FloatingSnapshot *snapshot) {
    if (!anto_floating_view_is_current(runtime)) return;
    g_autofree char *subtitle = anto_floating_subtitle(snapshot);
    gtk_label_set_text(GTK_LABEL(runtime->app->page_subtitle), subtitle);
    if (GTK_IS_LABEL(runtime->floating_badge))
        gtk_label_set_text(GTK_LABEL(runtime->floating_badge),
                           snapshot && snapshot->valid
                               ? snapshot->floating ? "FLOATING" : "TILED"
                               : "N/D");
    if (GTK_IS_LABEL(runtime->pin_badge))
        gtk_label_set_text(GTK_LABEL(runtime->pin_badge),
                           snapshot && snapshot->valid
                               ? snapshot->pinned ? "PIN" : "LIBERA"
                               : "N/D");
}

static void floating_received(const char *output, const GError *error, gboolean changed, gpointer data) {
    (void)changed;
    FloatingRuntime *runtime = data;
    if (runtime->app->closing) return;
    FloatingSnapshot *snapshot =
        output && !error ? anto_floating_snapshot_parse(output) : NULL;
    if (snapshot) {
        anto_floating_snapshot_free(runtime->snapshot);
        runtime->snapshot = snapshot;
        anto_floating_apply_snapshot(runtime, runtime->snapshot);
    }

}


void anto_floating_refresh_start(FloatingRuntime *runtime) {
    if (!runtime->query) {
        g_autofree char *backend = menu_backend_path();
        const char *argv[] = {backend, "floating", "snapshot", NULL};
        const char *override = g_getenv("ANTO_MENU_FLOATING_SNAPSHOT_CMD");
        const char *test_argv[] = {"/bin/sh", "-lc", override, NULL};
        runtime->query = anto_query_new(G_OBJECT(runtime->app->window), override && *override ? test_argv : argv, 5, floating_received, runtime);
    }
    anto_query_request(runtime->query);
}


void menu_floating_live_event(MenuApp *app) {
    if (!app || !app->window ||
        g_strcmp0(app->current_page, "floating") != 0)
        return;
    anto_floating_refresh_start(anto_floating_runtime_get(app));
}
