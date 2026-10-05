#include "internal.h"


void anto_keyboard_snapshot_free(KeyboardSnapshot *snapshot) {
    if (!snapshot) return;
    g_free(snapshot->layout);
    g_free(snapshot->keyboard);
    g_free(snapshot);
}

void anto_keyboard_runtime_free(gpointer data) {
    KeyboardRuntime *runtime = data;
    if (!runtime) return;
    anto_keyboard_snapshot_free(runtime->snapshot);
    g_weak_ref_clear(&runtime->root);
    anto_query_close(runtime->query);
    g_clear_object(&runtime->query);
    g_free(runtime);
}

KeyboardRuntime *anto_keyboard_runtime_get(MenuApp *app) {
    KeyboardRuntime *runtime =
        g_object_get_data(G_OBJECT(app->window), KEYBOARD_RUNTIME_KEY);
    if (runtime) return runtime;
    runtime = g_new0(KeyboardRuntime, 1);
    runtime->app = app;
    g_weak_ref_init(&runtime->root, NULL);
    g_object_set_data_full(G_OBJECT(app->window), KEYBOARD_RUNTIME_KEY,
                           runtime, anto_keyboard_runtime_free);
    return runtime;
}

const char *anto_keyboard_json_string(struct json_object *object, const char *key) {
    struct json_object *value = NULL;
    return json_object_object_get_ex(object, key, &value)
               ? json_object_get_string(value)
               : "";
}

KeyboardSnapshot *anto_keyboard_snapshot_parse(const char *output) {
    KeyboardSnapshot *snapshot = g_new0(KeyboardSnapshot, 1);
    const char marker[] = "\nDEVICES\n";
    const char *split = output ? strstr(output, marker) : NULL;
    if (!split) return snapshot;

    g_autofree char *header =
        g_strndup(output, (gsize)(split - output));
    g_auto(GStrv) lines = g_strsplit(header, "\n", -1);
    for (guint index = 0; lines[index]; index++) {
        g_auto(GStrv) fields = g_strsplit(lines[index], "\t", 2);
        if (fields[0] && fields[1] &&
            g_strcmp0(fields[0], "FCITX") == 0)
            snapshot->fcitx_active =
                g_strcmp0(fields[1], "active") == 0;
    }

    struct json_object *root =
        json_tokener_parse(split + strlen(marker));
    struct json_object *keyboards = NULL;
    if (!root || !json_object_is_type(root, json_type_object) ||
        !json_object_object_get_ex(root, "keyboards", &keyboards) ||
        !json_object_is_type(keyboards, json_type_array)) {
        if (root) json_object_put(root);
        return snapshot;
    }

    struct json_object *fallback = NULL;
    size_t count = json_object_array_length(keyboards);
    for (size_t index = 0; index < count; index++) {
        struct json_object *keyboard =
            json_object_array_get_idx(keyboards, index);
        if (!fallback) fallback = keyboard;
        struct json_object *main_value = NULL;
        gboolean main =
            json_object_object_get_ex(keyboard, "main", &main_value) &&
            json_object_get_boolean(main_value);
        if (!main) continue;
        fallback = keyboard;
        break;
    }

    if (fallback) {
        const char *layout = anto_keyboard_json_string(fallback, "active_keymap");
        const char *name = anto_keyboard_json_string(fallback, "name");
        snapshot->layout =
            g_strdup(*layout ? layout : "Layout sconosciuto");
        snapshot->keyboard =
            g_strdup(*name ? name : "Tastiera principale");
    } else {
        snapshot->layout = g_strdup("Layout sconosciuto");
        snapshot->keyboard = g_strdup("Nessuna tastiera rilevata");
    }
    snapshot->valid = TRUE;
    json_object_put(root);
    return snapshot;
}

GtkWidget *anto_keyboard_find_css_descendant(GtkWidget *widget,
                                      const char *css_class) {
    if (!widget) return NULL;
    if (gtk_widget_has_css_class(widget, css_class)) return widget;
    for (GtkWidget *child = gtk_widget_get_first_child(widget);
         child; child = gtk_widget_get_next_sibling(child)) {
        GtkWidget *match = anto_keyboard_find_css_descendant(child, css_class);
        if (match) return match;
    }
    return NULL;
}

GtkWidget *anto_keyboard_row_badge_at(MenuApp *app, int index) {
    GtkListBoxRow *row =
        gtk_list_box_get_row_at_index(GTK_LIST_BOX(app->list), index);
    return row
               ? anto_keyboard_find_css_descendant(GTK_WIDGET(row), "item-badge")
               : NULL;
}

char *anto_keyboard_subtitle(const KeyboardSnapshot *snapshot) {
    if (!snapshot || !snapshot->valid)
        return g_strdup("Layout in lettura · fcitx5 in verifica");
    return g_strdup_printf(
        "%s · fcitx5 %s · %s",
        snapshot->layout && *snapshot->layout
            ? snapshot->layout
            : "Layout sconosciuto",
        snapshot->fcitx_active ? "attivo" : "inattivo",
        snapshot->keyboard && *snapshot->keyboard
            ? snapshot->keyboard
            : "Tastiera principale");
}

void anto_keyboard_apply_snapshot(KeyboardRuntime *runtime,
                                    const KeyboardSnapshot *snapshot) {
    if (!anto_keyboard_view_is_current(runtime)) return;
    g_autofree char *subtitle = anto_keyboard_subtitle(snapshot);
    gtk_label_set_text(GTK_LABEL(runtime->app->page_subtitle),
                       subtitle);
    const char *layout =
        snapshot && snapshot->valid && snapshot->layout &&
                *snapshot->layout
            ? snapshot->layout
            : "N/D";
    const char *keyboard =
        snapshot && snapshot->valid && snapshot->keyboard &&
                *snapshot->keyboard
            ? snapshot->keyboard
            : "Tastiera principale";
    static const char *const layout_titles[] = {
        "Layout successivo", "Layout precedente",
    };
    gboolean search_changed = FALSE;
    for (guint index = 0;
         index < G_N_ELEMENTS(runtime->layout_badges); index++) {
        if (GTK_IS_LABEL(runtime->layout_badges[index]) &&
            g_strcmp0(
                gtk_label_get_text(
                    GTK_LABEL(runtime->layout_badges[index])),
                layout) != 0)
            gtk_label_set_text(
                GTK_LABEL(runtime->layout_badges[index]), layout);
        search_changed |= anto_keyboard_row_search_set(
            runtime->layout_rows[index], layout_titles[index],
            "Cambia layout su tutte le tastiere", layout, keyboard);
    }
    const char *fcitx =
        snapshot && snapshot->valid
            ? snapshot->fcitx_active ? "ATTIVO" : "SPENTO"
            : "N/D";
    if (GTK_IS_LABEL(runtime->fcitx_badge) &&
        g_strcmp0(
            gtk_label_get_text(GTK_LABEL(runtime->fcitx_badge)),
            fcitx) != 0)
        gtk_label_set_text(
            GTK_LABEL(runtime->fcitx_badge), fcitx);
    search_changed |= anto_keyboard_row_search_set(
        runtime->fcitx_row, "Configura input method",
        "Lingue, metodi di input e scorciatoie fcitx5", fcitx, NULL);
    if (search_changed)
        gtk_list_box_invalidate_filter(
            GTK_LIST_BOX(runtime->app->list));
}

static void keyboard_received(const char *output, const GError *error, gboolean changed, gpointer data) {
    (void)changed;
    KeyboardRuntime *runtime = data;
    if (runtime->app->closing) return;
    KeyboardSnapshot *snapshot =
        output && !error ? anto_keyboard_snapshot_parse(output) : NULL;
    if (snapshot && snapshot->valid) {
        anto_keyboard_snapshot_free(runtime->snapshot);
        runtime->snapshot = snapshot;
        anto_keyboard_apply_snapshot(runtime, runtime->snapshot);
    } else {
        anto_keyboard_snapshot_free(snapshot);
    }

}


void anto_keyboard_refresh_start(KeyboardRuntime *runtime) {
    if (!runtime->query) {
        g_autofree char *backend = menu_backend_path();
        const char *argv[] = {backend, "keyboard", "snapshot", NULL};
        const char *override = g_getenv("ANTO_MENU_KEYBOARD_SNAPSHOT_CMD");
        const char *test_argv[] = {"/bin/sh", "-lc", override, NULL};
        runtime->query = anto_query_new(G_OBJECT(runtime->app->window), override && *override ? test_argv : argv, 5, keyboard_received, runtime);
    }
    anto_query_request(runtime->query);
}
