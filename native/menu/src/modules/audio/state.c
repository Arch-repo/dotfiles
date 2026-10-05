#include "internal.h"

void anto_audio_snapshot_free(AudioSnapshot *snapshot) {
    if (!snapshot) return;
    g_free(snapshot->sink_name);
    g_free(snapshot->player);
    g_free(snapshot->status);
    g_free(snapshot->artist);
    g_free(snapshot->title);
    g_free(snapshot->album);
    g_free(snapshot);
}

void anto_audio_runtime_free(gpointer data) {
    AudioRuntime *runtime = data;
    if (!runtime) return;
    if (runtime->interaction_source)
        g_source_remove(runtime->interaction_source);
    anto_query_close(runtime->query);
    g_clear_object(&runtime->query);
    anto_audio_snapshot_free(runtime->snapshot);
    g_weak_ref_clear(&runtime->root);
    g_free(runtime);
}

AudioRuntime *anto_audio_runtime_get(MenuApp *app) {
    AudioRuntime *runtime =
        g_object_get_data(G_OBJECT(app->window), AUDIO_RUNTIME_KEY);
    if (runtime) return runtime;
    runtime = g_new0(AudioRuntime, 1);
    runtime->app = app;
    g_weak_ref_init(&runtime->root, NULL);
    g_object_set_data_full(G_OBJECT(app->window), AUDIO_RUNTIME_KEY, runtime,
                           anto_audio_runtime_free);
    return runtime;
}

gboolean anto_audio_root_is_current(AudioRuntime *runtime) {
    GtkWidget *root = g_weak_ref_get(&runtime->root);
    gboolean current = root && runtime->app->list &&
                       gtk_widget_is_ancestor(root, runtime->app->list);
    g_clear_object(&root);
    return current;
}

double anto_audio_parse_volume(const char *text, gboolean *muted,
                           gboolean *valid) {
    const char *colon = text ? strchr(text, ':') : NULL;
    char *end = NULL;
    double value = colon ? g_ascii_strtod(colon + 1, &end) : 0.0;
    *valid = colon && end && end != colon + 1;
    *muted = text && strstr(text, "MUTED") != NULL;
    return *valid ? CLAMP(value * 100.0, 0.0, 150.0) : 0.0;
}

AudioSnapshot *anto_audio_snapshot_parse(const char *output) {
    AudioSnapshot *snapshot = g_new0(AudioSnapshot, 1);
    g_auto(GStrv) lines = g_strsplit(output ? output : "", "\n", -1);
    for (guint index = 0; lines[index]; index++) {
        g_auto(GStrv) fields = g_strsplit(lines[index], "\t", -1);
        if (g_strcmp0(fields[0], "SINK") == 0 && fields[1]) {
            gboolean valid = FALSE;
            snapshot->sink_volume =
                anto_audio_parse_volume(fields[1], &snapshot->sink_muted, &valid);
            snapshot->parsed |= valid;
        } else if (g_strcmp0(fields[0], "SOURCE") == 0 && fields[1]) {
            gboolean valid = FALSE;
            snapshot->source_volume =
                anto_audio_parse_volume(fields[1], &snapshot->source_muted, &valid);
            snapshot->parsed |= valid;
        } else if (g_strcmp0(fields[0], "NAME") == 0 && fields[1]) {
            snapshot->sink_name = g_strdup(fields[1]);
        } else if (g_strcmp0(fields[0], "MPRIS") == 0 && fields[5]) {
            snapshot->player = g_strdup(fields[1]);
            snapshot->status = g_strdup(fields[2]);
            snapshot->artist = g_strdup(fields[3]);
            snapshot->title = g_strdup(fields[4]);
            snapshot->album = g_strdup(fields[5]);
        }
    }
    return snapshot;
}

char *anto_audio_player_text(const AudioSnapshot *snapshot) {
    if (!snapshot || ((!snapshot->title || !*snapshot->title) &&
                      (!snapshot->artist || !*snapshot->artist)))
        return g_strdup("Nessun brano attivo");
    GString *text = g_string_new(NULL);
    if (snapshot->artist && *snapshot->artist)
        g_string_append(text, snapshot->artist);
    if (snapshot->title && *snapshot->title) {
        if (text->len) g_string_append(text, " — ");
        g_string_append(text, snapshot->title);
    }
    if (snapshot->album && *snapshot->album)
        g_string_append_printf(text, " · %s", snapshot->album);
    if (snapshot->player && *snapshot->player)
        g_string_append_printf(text, " · %s", snapshot->player);
    if (snapshot->status && *snapshot->status)
        g_string_append_printf(text, " · %s", snapshot->status);
    return g_string_free(text, FALSE);
}

GtkWidget *anto_audio_find_css_descendant(GtkWidget *widget,
                                      const char *css_class) {
    if (!widget) return NULL;
    if (gtk_widget_has_css_class(widget, css_class)) return widget;
    for (GtkWidget *child = gtk_widget_get_first_child(widget); child;
         child = gtk_widget_get_next_sibling(child)) {
        GtkWidget *match = anto_audio_find_css_descendant(child, css_class);
        if (match) return match;
    }
    return NULL;
}

void anto_audio_apply_snapshot(AudioRuntime *runtime,
                                 const AudioSnapshot *snapshot) {
    if (!anto_audio_root_is_current(runtime)) return;
    if (!snapshot) {
        gtk_label_set_text(GTK_LABEL(runtime->app->page_subtitle),
                           "Rilevamento PipeWire e player in corso…");
        gtk_label_set_text(GTK_LABEL(runtime->sink_detail),
                           "Lettura dell’uscita predefinita…");
        gtk_label_set_text(GTK_LABEL(runtime->source_detail),
                           "Lettura del microfono predefinito…");
        gtk_widget_set_sensitive(runtime->sink_scale, FALSE);
        gtk_widget_set_sensitive(runtime->source_scale, FALSE);
        if (runtime->player_detail)
            gtk_label_set_text(GTK_LABEL(runtime->player_detail),
                               "Lettura dei metadati MPRIS…");
        if (anto_audio_player_search_set(
                runtime, "Lettura dei metadati MPRIS…"))
            gtk_list_box_invalidate_filter(
                GTK_LIST_BOX(runtime->app->list));
        return;
    }
    gtk_widget_set_sensitive(runtime->sink_scale, TRUE);
    gtk_widget_set_sensitive(runtime->source_scale, TRUE);
    const char *name = snapshot->sink_name && *snapshot->sink_name
                           ? snapshot->sink_name
                           : "Uscita predefinita";
    g_autofree char *subtitle = g_strdup_printf(
        "%s · Volume %.0f%%%s", name, snapshot->sink_volume,
        snapshot->sink_muted ? " · muto" : "");
    gtk_label_set_text(GTK_LABEL(runtime->app->page_subtitle), subtitle);
    gtk_image_set_from_icon_name(
        GTK_IMAGE(runtime->sink_icon),
        snapshot->sink_muted ? "audio-volume-muted-symbolic"
                             : "audio-volume-high-symbolic");
    gtk_image_set_from_icon_name(
        GTK_IMAGE(runtime->source_icon),
        snapshot->source_muted ? "microphone-disabled-symbolic"
                               : "audio-input-microphone-symbolic");
    gtk_label_set_text(GTK_LABEL(runtime->sink_detail),
                       snapshot->sink_muted ? "Attualmente disattivato"
                                            : "Fino al 150%");
    gtk_label_set_text(GTK_LABEL(runtime->source_detail),
                       snapshot->source_muted ? "Microfono disattivato"
                                              : "Ingresso predefinito");
    runtime->applying = TRUE;
    gtk_range_set_value(GTK_RANGE(runtime->sink_scale),
                        snapshot->sink_volume);
    gtk_range_set_value(GTK_RANGE(runtime->source_scale),
                        snapshot->source_volume);
    runtime->applying = FALSE;
    g_autofree char *player = anto_audio_player_text(snapshot);
    if (runtime->player_detail)
        gtk_label_set_text(GTK_LABEL(runtime->player_detail), player);
    if (anto_audio_player_search_set(runtime, player))
        gtk_list_box_invalidate_filter(
            GTK_LIST_BOX(runtime->app->list));
}



static void audio_received(const char *output, const GError *error, gboolean changed, gpointer data) {
    AudioRuntime *runtime = data;
    if (error || !output || runtime->app->closing) return;
    if (changed) {
        AudioSnapshot *snapshot = anto_audio_snapshot_parse(output);
        if (snapshot && snapshot->parsed) {
            anto_audio_snapshot_free(runtime->snapshot);
            runtime->snapshot = snapshot;
        } else anto_audio_snapshot_free(snapshot);
    }
    if (runtime->snapshot && anto_audio_root_is_current(runtime) && runtime->full_view) {
        if (runtime->interacting) runtime->refresh_pending = TRUE;
        else anto_audio_apply_snapshot(runtime, runtime->snapshot);
    }
}


void anto_audio_start_snapshot(MenuApp *app) {
    AudioRuntime *runtime = anto_audio_runtime_get(app);
    if (runtime->interacting) { runtime->refresh_pending = TRUE; return; }
    runtime->refresh_pending = FALSE;
    if (!runtime->query) {
        g_autofree char *backend = menu_backend_path();
        const char *argv[] = {backend, "audio", "snapshot", NULL};
        runtime->query = anto_query_new(G_OBJECT(app->window), argv, 5, audio_received, runtime);
    }
    anto_query_request(runtime->query);
}


void menu_audio_live_event(MenuApp *app) {
    if (!app || !app->window ||
        g_strcmp0(app->current_page, "audio") != 0)
        return;
    anto_audio_start_snapshot(app);
}
