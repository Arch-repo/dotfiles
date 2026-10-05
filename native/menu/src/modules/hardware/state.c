#include "internal.h"

char *anto_hardware_performance_action_path(void) {
    return menu_backend_path();
}

void anto_hardware_snapshot_free(HardwareSnapshot *snapshot) {
    if (!snapshot) return;
    g_free(snapshot->cpu);
    g_free(snapshot->memory);
    g_free(snapshot->disk);
    g_free(snapshot->temperature);
    g_free(snapshot->kernel);
    g_free(snapshot->profile);
    g_free(snapshot);
}

void anto_hardware_runtime_free(gpointer data) {
    HardwareRuntime *runtime = data;
    if (!runtime) return;
    if (runtime->profile_process)
        g_subprocess_force_exit(runtime->profile_process);
    g_clear_object(&runtime->profile_process);
    anto_hardware_snapshot_free(runtime->snapshot);
    g_weak_ref_clear(&runtime->root);
    anto_query_close(runtime->query);
    g_clear_object(&runtime->query);
    g_free(runtime);
}

HardwareRuntime *anto_hardware_runtime_get(MenuApp *app) {
    HardwareRuntime *runtime =
        g_object_get_data(G_OBJECT(app->window), HARDWARE_RUNTIME_KEY);
    if (runtime) return runtime;
    runtime = g_new0(HardwareRuntime, 1);
    runtime->app = app;
    g_weak_ref_init(&runtime->root, NULL);
    g_object_set_data_full(G_OBJECT(app->window), HARDWARE_RUNTIME_KEY,
                           runtime, anto_hardware_runtime_free);
    return runtime;
}

gboolean anto_hardware_root_is_current(HardwareRuntime *runtime) {
    GtkWidget *root = g_weak_ref_get(&runtime->root);
    gboolean current = root && runtime->app->list &&
                       gtk_widget_is_ancestor(root, runtime->app->list);
    g_clear_object(&root);
    return current;
}

HardwareSnapshot *anto_hardware_snapshot_parse(const char *output) {
    HardwareSnapshot *snapshot = g_new0(HardwareSnapshot, 1);
    g_auto(GStrv) lines = g_strsplit(output ? output : "", "\n", -1);
    for (guint index = 0; lines[index]; index++) {
        g_auto(GStrv) fields = g_strsplit(lines[index], "\t", 2);
        if (!fields[0] || !fields[1]) continue;
        char **slot = NULL;
        if (g_strcmp0(fields[0], "CPU") == 0) slot = &snapshot->cpu;
        else if (g_strcmp0(fields[0], "MEMORY") == 0) slot = &snapshot->memory;
        else if (g_strcmp0(fields[0], "DISK") == 0) slot = &snapshot->disk;
        else if (g_strcmp0(fields[0], "TEMP") == 0) slot = &snapshot->temperature;
        else if (g_strcmp0(fields[0], "KERNEL") == 0) slot = &snapshot->kernel;
        else if (g_strcmp0(fields[0], "PROFILE") == 0) slot = &snapshot->profile;
        if (slot) {
            g_free(*slot);
            *slot = g_strdup(fields[1]);
            snapshot->parsed = TRUE;
        }
    }
    return snapshot;
}

const char *anto_hardware_present(const char *text, const char *fallback) {
    return text && *text ? text : fallback;
}

GtkWidget *anto_hardware_find_css_descendant(GtkWidget *widget,
                                      const char *css_class) {
    if (!widget) return NULL;
    if (gtk_widget_has_css_class(widget, css_class)) return widget;
    for (GtkWidget *child = gtk_widget_get_first_child(widget); child;
         child = gtk_widget_get_next_sibling(child)) {
        GtkWidget *match = anto_hardware_find_css_descendant(child, css_class);
        if (match) return match;
    }
    return NULL;
}

void anto_hardware_apply_snapshot(HardwareRuntime *runtime,
                                    const HardwareSnapshot *snapshot) {
    const HardwareSnapshot empty = {0};
    if (!anto_hardware_root_is_current(runtime) || !runtime->full_view)
        return;
    if (!snapshot) snapshot = &empty;
    static const char *const metric_titles[] = {
        "Carico CPU", "Memoria", "Disco di sistema", "Temperatura",
    };
    const char *metric_values[] = {
        anto_hardware_present(snapshot->cpu, "n/d"),
        anto_hardware_present(snapshot->memory, "n/d"),
        anto_hardware_present(snapshot->disk, "n/d"),
        anto_hardware_present(snapshot->temperature, "Sensore non esposto"),
    };
    GtkWidget *metric_details[] = {
        runtime->cpu_detail,
        runtime->memory_detail,
        runtime->disk_detail,
        runtime->temperature_detail,
    };
    gboolean search_changed = FALSE;
    for (guint index = 0; index < G_N_ELEMENTS(metric_values); index++) {
        if (GTK_IS_LABEL(metric_details[index]) &&
            g_strcmp0(gtk_label_get_text(GTK_LABEL(metric_details[index])),
                      metric_values[index]) != 0)
            gtk_label_set_text(GTK_LABEL(metric_details[index]),
                               metric_values[index]);
        search_changed |= anto_hardware_row_search_set(
            runtime->metric_rows[index], metric_titles[index],
            metric_values[index], index == 0 ? "LIVE" : NULL);
    }
    g_autofree char *subtitle = g_strdup_printf(
        "Kernel %s · temperatura %s · profilo %s",
        anto_hardware_present(snapshot->kernel, "n/d"),
        anto_hardware_present(snapshot->temperature, "n/d"),
        anto_hardware_present(snapshot->profile, "non disponibile"));
    gtk_label_set_text(GTK_LABEL(runtime->app->page_subtitle), subtitle);

    static const char *const profiles[] = {
        "power-saver", "balanced", "performance",
    };
    static const char *const profile_titles[] = {
        "Silenzioso", "Bilanciato", "Prestazioni",
    };
    static const char *const profile_subtitles[] = {
        "Riduce consumi e calore",
        "Gestione automatica quotidiana",
        "Favorisce potenza e reattività",
    };
    for (guint index = 0; index < G_N_ELEMENTS(profiles); index++) {
        gboolean active =
            g_strcmp0(snapshot->profile, profiles[index]) == 0;
        gtk_label_set_text(GTK_LABEL(runtime->profile_badges[index]),
                           active ? "ATTIVO" : "");
        gtk_widget_set_visible(runtime->profile_badges[index], active);
        search_changed |= anto_hardware_row_search_set(
            runtime->profile_rows[index], profile_titles[index],
            profile_subtitles[index], active ? "ATTIVO" : NULL);
    }
    if (search_changed)
        gtk_list_box_invalidate_filter(
            GTK_LIST_BOX(runtime->app->list));
}

void anto_hardware_pending_free(HardwarePending *pending) {
    if (!pending) return;
    g_weak_ref_clear(&pending->window);
    g_clear_object(&pending->process);
    g_free(pending);
}

static void hardware_received(const char *output, const GError *error, gboolean changed, gpointer data) {
    (void)changed;
    HardwareRuntime *runtime = data;
    if (runtime->app->closing) return;
    HardwareSnapshot *snapshot =
        output && !error ? anto_hardware_snapshot_parse(output) : NULL;
    if (snapshot && snapshot->parsed) {
        anto_hardware_snapshot_free(runtime->snapshot);
        runtime->snapshot = snapshot;
        if (g_strcmp0(runtime->app->current_page, "hardware") == 0) {
            if (anto_hardware_root_is_current(runtime) &&
                runtime->full_view)
                anto_hardware_apply_snapshot(runtime, runtime->snapshot);
        }
    } else {
        anto_hardware_snapshot_free(snapshot);
    }
}

void anto_hardware_profile_finished(GObject *object, GAsyncResult *result, gpointer data) {
    HardwarePending *pending = data;
    g_autofree char *output = NULL, *diagnostic = NULL;
    g_autoptr(GError) error = NULL;
    gboolean ok = g_subprocess_communicate_utf8_finish(G_SUBPROCESS(object), result, &output, &diagnostic, &error);
    ok = ok && g_subprocess_get_successful(G_SUBPROCESS(object));
    g_autoptr(GObject) window = g_weak_ref_get(&pending->window);
    HardwareRuntime *runtime = window ? g_object_get_data(window, HARDWARE_RUNTIME_KEY) : NULL;
    if (runtime && runtime->profile_process == G_SUBPROCESS(object)) {
        g_clear_object(&runtime->profile_process);
        if (!runtime->app->closing) {
            if (!ok) menu_notify("Profilo energetico", "Il profilo non è stato applicato");
            if (g_strcmp0(runtime->app->current_page, "hardware") == 0)
                anto_hardware_start_snapshot(runtime->app);
        }
    }
    anto_hardware_pending_free(pending);
}
void anto_hardware_profile_action(MenuApp *app, gpointer data) {
    HardwareProfileAction *action = data;
    HardwareRuntime *runtime = action->runtime;
    if (runtime->profile_process || app->closing) return;
    g_autofree char *backend = menu_backend_path();
    g_autoptr(GError) error = NULL;
    runtime->profile_process = g_subprocess_new(G_SUBPROCESS_FLAGS_STDOUT_PIPE | G_SUBPROCESS_FLAGS_STDERR_PIPE,
        &error, "/usr/bin/timeout", "--foreground", "--kill-after=1", "8", backend, "energy", "set", action->profile, NULL);
    if (!runtime->profile_process) { menu_notify("Profilo energetico", "Servizio non disponibile"); return; }
    HardwarePending *pending = g_new0(HardwarePending, 1);
    g_weak_ref_init(&pending->window, G_OBJECT(app->window));
    pending->process = g_object_ref(runtime->profile_process);
    g_subprocess_communicate_utf8_async(runtime->profile_process, NULL, NULL, anto_hardware_profile_finished, pending);
}
void anto_hardware_start_snapshot(MenuApp *app) {
    HardwareRuntime *runtime = anto_hardware_runtime_get(app);
    if (!runtime->query) {
        g_autofree char *backend = menu_backend_path();
        const char *argv[] = {backend, "system", "hardware-snapshot", NULL};
        runtime->query = anto_query_new(G_OBJECT(app->window), argv, 8, hardware_received, runtime);
    }
    anto_query_request(runtime->query);
}
void menu_hardware_live_event(MenuApp *app) {
    if (app && !app->closing && g_strcmp0(app->current_page, "hardware") == 0)
        anto_hardware_start_snapshot(app);
}
