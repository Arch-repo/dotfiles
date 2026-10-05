#include "internal.h"

gboolean anto_audio_interaction_finished(gpointer data) {
    AudioRuntime *runtime = data;
    runtime->interaction_source = 0;
    runtime->interacting = FALSE;
    if (runtime->refresh_pending &&
        g_strcmp0(runtime->app->current_page, "audio") == 0) {
        runtime->refresh_pending = FALSE;
        anto_audio_start_snapshot(runtime->app);
    }
    return G_SOURCE_REMOVE;
}

void anto_audio_scale_changed(GtkRange *range, gpointer data) {
    AudioScaleBinding *binding = data;
    AudioRuntime *runtime = binding->runtime;
    if (runtime->applying) return;
    runtime->interacting = TRUE;
    if (runtime->interaction_source)
        g_source_remove(runtime->interaction_source);
    runtime->interaction_source =
        g_timeout_add(450, anto_audio_interaction_finished, runtime);
    g_autofree char *value = g_strdup_printf(
        "%.3f", CLAMP(gtk_range_get_value(range), 0.0, 150.0) / 100.0);
    menu_spawn_backend(runtime->app, "audio", "volume",
                       binding->target, value, FALSE);
}

void anto_audio_scale_binding_free(gpointer data, GClosure *closure) {
    (void)closure;
    g_free(data);
}
