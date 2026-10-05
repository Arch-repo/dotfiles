#include "internal.h"

char *anto_brightness_performance_action_path(void) {
    return menu_backend_path();
}

void anto_brightness_snapshot_free(BrightnessSnapshot *snapshot) {
    if (!snapshot) return;
    g_free(snapshot->capacity);
    g_free(snapshot->status);
    g_free(snapshot->profile);
    g_free(snapshot);
}

void anto_brightness_replace_text(char **target, const char *value) {
    g_free(*target);
    *target = g_strdup(value ? value : "");
}

BrightnessSnapshot *anto_brightness_snapshot_parse(const char *output) {
    BrightnessSnapshot *snapshot = g_new0(BrightnessSnapshot, 1);
    g_auto(GStrv) lines = g_strsplit(output ? output : "", "\n", -1);
    for (guint index = 0; lines[index]; index++) {
        if (!*lines[index]) continue;
        g_auto(GStrv) fields = g_strsplit(lines[index], "\t", 2);
        if (!fields[0] || !fields[1]) continue;
        g_strstrip(fields[1]);
        if (g_strcmp0(fields[0], "brightness") == 0) {
            char *end = NULL;
            double value = g_ascii_strtod(fields[1], &end);
            if (end && end != fields[1] && isfinite(value)) {
                snapshot->brightness = CLAMP(value, 0.0, 100.0);
                snapshot->has_brightness = TRUE;
            }
        } else if (g_strcmp0(fields[0], "capacity") == 0) {
            anto_brightness_replace_text(&snapshot->capacity, fields[1]);
        } else if (g_strcmp0(fields[0], "status") == 0) {
            anto_brightness_replace_text(&snapshot->status, fields[1]);
        } else if (g_strcmp0(fields[0], "profile") == 0) {
            anto_brightness_replace_text(&snapshot->profile, fields[1]);
        }
    }
    snapshot->valid = snapshot->has_brightness || snapshot->capacity ||
                      snapshot->status || snapshot->profile;
    return snapshot;
}

const char *anto_brightness_snapshot_text(const char *text, const char *fallback) {
    return text && *text ? text : fallback;
}

GtkWidget *anto_brightness_find(GtkWidget *root, const char *css_class,
                                  gboolean find_scale) {
    if (!root) return NULL;
    if ((find_scale && GTK_IS_SCALE(root)) ||
        (!find_scale && gtk_widget_has_css_class(root, css_class)))
        return root;
    for (GtkWidget *child = gtk_widget_get_first_child(root);
         child; child = gtk_widget_get_next_sibling(child)) {
        GtkWidget *match = anto_brightness_find(child, css_class, find_scale);
        if (match) return match;
    }
    return NULL;
}

void anto_brightness_live_free(gpointer data) {
    BrightnessLive *live = data;
    if (!live) return;
    if (live->debounce_source)
        g_source_remove(live->debounce_source);
    if (live->interaction_source)
        g_source_remove(live->interaction_source);
    if (live->profile_cancellable)
        g_cancellable_cancel(live->profile_cancellable);
    if (live->profile_process)
        g_subprocess_force_exit(live->profile_process);
    g_clear_object(&live->profile_process);
    g_clear_object(&live->profile_cancellable);
    anto_brightness_snapshot_free(live->snapshot);
    anto_query_close(live->query);
    g_clear_object(&live->query);
    g_free(live);
}

BrightnessLive *anto_brightness_live_get(MenuApp *app) {
    BrightnessLive *live =
        g_object_get_data(G_OBJECT(app->window), BRIGHTNESS_LIVE_KEY);
    if (live) return live;
    live = g_new0(BrightnessLive, 1);
    live->app = app;
    g_object_set_data_full(G_OBJECT(app->window), BRIGHTNESS_LIVE_KEY,
                           live, anto_brightness_live_free);
    return live;
}

void anto_brightness_operation_free(BrightnessOperation *operation) {
    if (!operation) return;
    g_weak_ref_clear(&operation->window);
    g_clear_object(&operation->process);
    g_free(operation);
}

void anto_brightness_performance_finished(GObject *object, GAsyncResult *result,
                                 gpointer data) {
    BrightnessOperation *operation = data;
    char *output = NULL;
    char *error_text = NULL;
    g_autoptr(GError) error = NULL;
    gboolean ok = g_subprocess_communicate_utf8_finish(
        G_SUBPROCESS(object), result, &output, &error_text, &error);
    ok = ok && g_subprocess_get_successful(G_SUBPROCESS(object));

    GtkWidget *window = g_weak_ref_get(&operation->window);
    if (window) {
        BrightnessLive *live =
            g_object_get_data(G_OBJECT(window), BRIGHTNESS_LIVE_KEY);
        if (live && live->profile_process == G_SUBPROCESS(object)) {
            g_clear_object(&live->profile_process);
            g_clear_object(&live->profile_cancellable);
            anto_brightness_profile_rows_set_sensitive(live, TRUE);
            if (!ok) {
                if (error_text) g_strstrip(error_text);
                menu_notify(
                    "Profilo energetico",
                    error_text && *error_text ? error_text :
                    error ? error->message :
                    "Il profilo non è stato applicato");
            }
            menu_brightness_live_event(live->app);
        }
    }
    g_clear_object(&window);
    g_free(output);
    g_free(error_text);
    anto_brightness_operation_free(operation);
}

void anto_brightness_set_performance_profile(MenuApp *app, gpointer data) {
    const char *profile = data;
    BrightnessLive *live = anto_brightness_live_get(app);
    if (live->profile_process) return;
    g_autofree char *script = anto_brightness_performance_action_path();
    const char *argv[] = {
        "/usr/bin/timeout", "--foreground", "--kill-after=1", "8",
        script, "energy", "set", profile, NULL,
    };
    g_autoptr(GError) error = NULL;
    live->profile_process = g_subprocess_newv(
        argv, G_SUBPROCESS_FLAGS_STDOUT_PIPE |
                  G_SUBPROCESS_FLAGS_STDERR_PIPE,
        &error);
    if (!live->profile_process) {
        menu_notify("Profilo energetico",
                    error ? error->message :
                    "Impossibile avviare il controllo energetico");
        return;
    }
    live->profile_cancellable = g_cancellable_new();
    anto_brightness_profile_rows_set_sensitive(live, FALSE);
    BrightnessOperation *operation = g_new0(BrightnessOperation, 1);
    g_weak_ref_init(&operation->window, G_OBJECT(app->window));
    operation->process = g_object_ref(live->profile_process);
    g_subprocess_communicate_utf8_async(
        live->profile_process, NULL, live->profile_cancellable,
        anto_brightness_performance_finished, operation);
}

void anto_brightness_set_brightness(MenuApp *app, double value, gpointer data) {
    (void)data;
    BrightnessLive *live = anto_brightness_live_get(app);
    if (live->updating_widgets) return;
    live->scale_active = TRUE;
    if (!live->pointer_active) {
        if (live->interaction_source)
            g_source_remove(live->interaction_source);
        live->interaction_source =
            g_timeout_add(650, anto_brightness_interaction_fire, live);
    }
    int percent = (int)round(CLAMP(value, 1.0, 100.0));
    g_autofree char *requested = g_strdup_printf("%d", percent);
    menu_spawn_backend(app, "energy", "brightness-set", requested,
                       NULL, FALSE);
    /* The interaction timer performs one authoritative refresh after the
     * last pointer or keyboard change. */
}

char *anto_brightness_subtitle(const BrightnessSnapshot *snapshot) {
    double brightness = snapshot->has_brightness ?
                        snapshot->brightness : 1.0;
    const char *capacity = anto_brightness_snapshot_text(snapshot->capacity, "?");
    const char *status = anto_brightness_snapshot_text(snapshot->status, "stato sconosciuto");
    const char *profile =
        anto_brightness_snapshot_text(snapshot->profile, "non disponibile");
    const char *localized_status = anto_brightness_battery_status_label(status);
    const char *localized_profile = anto_brightness_profile_label(profile);
    return snapshot->has_brightness ?
        g_strdup_printf("Schermo %.0f%% · Batteria %s%% · %s · %s",
                        brightness, capacity, localized_status,
                        localized_profile) :
        g_strdup_printf("Luminosità n/d · Batteria %s%% · %s · %s",
                        capacity, localized_status, localized_profile);
}

void anto_brightness_apply_snapshot(BrightnessLive *live,
                                      const BrightnessSnapshot *snapshot) {
    g_autofree char *subtitle = anto_brightness_subtitle(snapshot);
    gtk_label_set_text(GTK_LABEL(live->app->page_subtitle), subtitle);
    gboolean search_changed = FALSE;
    live->updating_widgets = TRUE;
    if (GTK_IS_RANGE(live->scale)) {
        gtk_widget_set_sensitive(live->scale, snapshot->has_brightness);
        if (snapshot->has_brightness && !live->scale_active)
            gtk_range_set_value(GTK_RANGE(live->scale),
                                snapshot->brightness);
    }
    live->updating_widgets = FALSE;
    if (GTK_IS_LABEL(live->scale_detail))
        gtk_label_set_text(
            GTK_LABEL(live->scale_detail),
            snapshot->has_brightness
                ? "Regolazione continua del pannello interno"
                : "In attesa del controllo di retroilluminazione");
    g_autofree char *scale_search = snapshot->has_brightness
        ? g_strdup_printf(
              "Luminosità schermo %.0f%% regolazione pannello interno",
              snapshot->brightness)
        : g_strdup("Luminosità schermo non disponibile "
                   "retroilluminazione pannello interno");
    search_changed |=
        anto_brightness_set_search(live->scale_row, scale_search);

    const char *profile =
        anto_brightness_snapshot_text(snapshot->profile, "non disponibile");
    const char *const ids[] = {"power-saver", "balanced", "performance"};
    const char *const titles[] = {
        "Risparmio energetico",
        "Bilanciato",
        "Prestazioni",
    };
    const char *const details[] = {
        "Massima autonomia e temperature contenute",
        "Profilo quotidiano consigliato",
        "Potenza massima quando serve",
    };
    for (guint i = 0; i < G_N_ELEMENTS(ids); i++) {
        gboolean active = g_strcmp0(profile, ids[i]) == 0;
        if (GTK_IS_LABEL(live->profile_badges[i]))
            gtk_label_set_text(GTK_LABEL(live->profile_badges[i]),
                               active ? "ATTIVO" : "");
        if (live->profile_badges[i])
            gtk_widget_set_visible(live->profile_badges[i], active);
        if (live->profile_rows[i])
            gtk_widget_set_sensitive(live->profile_rows[i],
                                     live->profile_process == NULL);
        g_autofree char *profile_search = g_strdup_printf(
            "%s %s %s %s", titles[i], details[i], ids[i],
            active ? "attivo" : "");
        search_changed |= anto_brightness_set_search(
            live->profile_rows[i], profile_search);
    }
    if (search_changed)
        gtk_list_box_invalidate_filter(GTK_LIST_BOX(live->app->list));
}

static void brightness_received(const char *output, const GError *error, gboolean changed, gpointer data) {
    (void)changed;
    BrightnessLive *live = data;
    if (live->app->closing) return;
    BrightnessSnapshot *snapshot =
        output && !error ? anto_brightness_snapshot_parse(output) : NULL;
    if (snapshot && snapshot->valid) {
        gboolean on_page =
            g_strcmp0(live->app->current_page, "brightness") == 0;
        anto_brightness_snapshot_free(live->snapshot);
        live->snapshot = snapshot;
        if (on_page && live->mounted)
            anto_brightness_apply_snapshot(live, live->snapshot);
    } else {
        anto_brightness_snapshot_free(snapshot);
    }

}


void anto_brightness_refresh_start(BrightnessLive *live) {
    if (!live->query) {
        g_autofree char *backend = menu_backend_path();
        const char *argv[] = {backend, "energy", "snapshot", NULL};
        live->query = anto_query_new(G_OBJECT(live->app->window), argv, 8, brightness_received, live);
    }
    anto_query_request(live->query);
}


gboolean anto_brightness_debounce_fire(gpointer data) {
    BrightnessLive *live = data;
    live->debounce_source = 0;
    if (g_strcmp0(live->app->current_page, "brightness") == 0)
        anto_brightness_refresh_start(live);
    return G_SOURCE_REMOVE;
}

void menu_brightness_live_event(MenuApp *app) {
    if (!app || !app->window ||
        g_strcmp0(app->current_page, "brightness") != 0)
        return;
    BrightnessLive *live = anto_brightness_live_get(app);
    if (live->debounce_source) return;
    live->debounce_source =
        g_timeout_add(450, anto_brightness_debounce_fire, live);
}
