#include "internal.h"

void anto_system_snapshot_free(SystemSnapshot *snapshot) {
    if (!snapshot) return;
    g_free(snapshot->volume);
    g_free(snapshot->network);
    g_free(snapshot->bluetooth);
    g_free(snapshot->brightness);
    g_free(snapshot->displays);
    g_free(snapshot);
}

void anto_system_replace_text(char **target, const char *value) {
    g_free(*target);
    *target = g_strdup(value ? value : "");
}

SystemSnapshot *anto_system_snapshot_parse(const char *output) {
    SystemSnapshot *snapshot = g_new0(SystemSnapshot, 1);
    g_auto(GStrv) lines = g_strsplit(output ? output : "", "\n", -1);
    for (guint index = 0; lines[index]; index++) {
        if (!*lines[index]) continue;
        g_auto(GStrv) fields = g_strsplit(lines[index], "\t", 2);
        if (!fields[0] || !fields[1]) continue;
        g_strstrip(fields[1]);
        if (g_strcmp0(fields[0], "volume") == 0)
            anto_system_replace_text(&snapshot->volume, fields[1]);
        else if (g_strcmp0(fields[0], "network") == 0)
            anto_system_replace_text(&snapshot->network, fields[1]);
        else if (g_strcmp0(fields[0], "bluetooth") == 0)
            anto_system_replace_text(&snapshot->bluetooth, fields[1]);
        else if (g_strcmp0(fields[0], "brightness") == 0)
            anto_system_replace_text(&snapshot->brightness, fields[1]);
        else if (g_strcmp0(fields[0], "displays") == 0)
            anto_system_replace_text(&snapshot->displays, fields[1]);
    }
    snapshot->valid = snapshot->volume || snapshot->network ||
                      snapshot->bluetooth || snapshot->brightness ||
                      snapshot->displays;
    return snapshot;
}

const char *anto_system_snapshot_text(const char *text, const char *fallback) {
    return text && *text ? text : fallback;
}

GtkWidget *anto_system_find_widget_with_class(GtkWidget *root,
                                         const char *css_class) {
    if (!root) return NULL;
    if (gtk_widget_has_css_class(root, css_class)) return root;
    for (GtkWidget *child = gtk_widget_get_first_child(root);
         child; child = gtk_widget_get_next_sibling(child)) {
        GtkWidget *match = anto_system_find_widget_with_class(child, css_class);
        if (match) return match;
    }
    return NULL;
}

GtkWidget *anto_system_tile_at(MenuApp *app, int index) {
    GtkFlowBoxChild *child =
        gtk_flow_box_get_child_at_index(GTK_FLOW_BOX(app->grid), index);
    return child ? GTK_WIDGET(child) : NULL;
}

void anto_system_apply_snapshot(SystemLive *live,
                                  const SystemSnapshot *snapshot) {
    static const char *const titles[] = {
        "Audio", "Wi‑Fi", "Bluetooth", "Energia", "Schermi",
    };
    static const char *const subtitles[] = {
        "Volume, microfono e player",
        "Reti e profili salvati",
        "Cuffie e dispositivi",
        "Luminosità e profilo",
        "Layout, profili e mirroring",
    };
    static const char *const fallbacks[] = {
        "n/d", "Disconnessa", "n/d", "n/d", "n/d",
    };
    const char *values[] = {
        snapshot ? snapshot->volume : NULL,
        snapshot ? snapshot->network : NULL,
        snapshot ? snapshot->bluetooth : NULL,
        snapshot ? snapshot->brightness : NULL,
        snapshot ? snapshot->displays : NULL,
    };
    gboolean search_changed = FALSE;
    for (guint i = 0; i < G_N_ELEMENTS(values); i++) {
        const char *value = anto_system_snapshot_text(values[i], fallbacks[i]);
        if (GTK_IS_LABEL(live->badges[i]) &&
            g_strcmp0(gtk_label_get_text(GTK_LABEL(live->badges[i])),
                      value) != 0)
            gtk_label_set_text(GTK_LABEL(live->badges[i]), value);
        search_changed |= anto_system_tile_search_set(
            live->tiles[i], titles[i], subtitles[i], value);
    }
    if (search_changed)
        gtk_flow_box_invalidate_filter(
            GTK_FLOW_BOX(live->app->grid));
}

void anto_system_live_free(gpointer data) {
    SystemLive *live = data;
    if (!live) return;
    anto_system_snapshot_free(live->snapshot);
    g_free(live);
}

SystemLive *anto_system_live_get(MenuApp *app) {
    SystemLive *live =
        g_object_get_data(G_OBJECT(app->window), SYSTEM_LIVE_KEY);
    if (live) return live;
    live = g_new0(SystemLive, 1);
    live->app = app;
    g_object_set_data_full(G_OBJECT(app->window), SYSTEM_LIVE_KEY,
                           live, anto_system_live_free);
    return live;
}

void menu_system_accept(MenuApp *app, const char *output) {
    SystemLive *live = anto_system_live_get(app);
    SystemSnapshot *snapshot = anto_system_snapshot_parse(output);
    if (!snapshot->valid) { anto_system_snapshot_free(snapshot); return; }
    anto_system_snapshot_free(live->snapshot);
    live->snapshot = snapshot;
    if (live->mounted && g_strcmp0(app->current_page, "system") == 0)
        anto_system_apply_snapshot(live, snapshot);
}
void anto_system_refresh_start(SystemLive *live) {
    menu_context_status_refresh(live->app);
}
void menu_system_live_event(MenuApp *app) {
    if (app && !app->closing) menu_context_status_refresh(app);
}
