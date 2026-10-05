#include "internal.h"
#include "primitives.h"

gboolean anto_audio_player_search_set(AudioRuntime *runtime,
                                        const char *detail) {
    if (!runtime->player_row) return FALSE;
    g_autofree char *combined = g_strdup_printf(
        "In riproduzione %s MPRIS", detail ? detail : "");
    g_autofree char *search = g_utf8_strdown(combined, -1);
    const char *current = g_object_get_data(
        G_OBJECT(runtime->player_row), "menu-search");
    if (g_strcmp0(current, search) == 0) return FALSE;
    g_object_set_data_full(G_OBJECT(runtime->player_row), "menu-search",
                           g_steal_pointer(&search), g_free);
    return TRUE;
}

GtkWidget *anto_audio_scale_row(AudioRuntime *runtime, const char *icon_name, const char *title,
                                  double maximum, GtkWidget **icon_out, GtkWidget **detail_out,
                                  GtkWidget **scale_out, const char *target) {
    GtkWidget *card = anto_ui_slider_card(icon_name, title, "Caricamento…", 0, 0, maximum, 1, icon_out, detail_out, scale_out);
    AudioScaleBinding *binding = g_new0(AudioScaleBinding, 1);
    *binding = (AudioScaleBinding){runtime, target};
    g_signal_connect_data(*scale_out, "value-changed", G_CALLBACK(anto_audio_scale_changed), binding, anto_audio_scale_binding_free, 0);
    menu_append_widget(runtime->app, card);
    return card;
}


void anto_audio_render(AudioRuntime *runtime,
                         const AudioSnapshot *snapshot) {
    MenuApp *app = runtime->app;
    runtime->player_row = NULL;
    runtime->player_detail = NULL;
    menu_page_begin(app, "audio-speakers-symbolic", "Audio",
                    "Uscita, microfono e riproduzione",
                    "Cerca un controllo audio…");
    GtkWidget *root = anto_audio_scale_row(
        runtime, "audio-volume-high-symbolic", "Volume uscita", 150,
        &runtime->sink_icon, &runtime->sink_detail, &runtime->sink_scale,
        "@DEFAULT_AUDIO_SINK@");
    anto_audio_scale_row(runtime, "audio-input-microphone-symbolic",
                    "Volume microfono", 100, &runtime->source_icon,
                    &runtime->source_detail, &runtime->source_scale,
                    "@DEFAULT_AUDIO_SOURCE@");

    menu_add_section(app, "AZIONI RAPIDE");
    menu_add_backend_item(
        app, "audio-volume-muted-symbolic", "Mute uscita",
        "Attiva o disattiva immediatamente gli altoparlanti",
        NULL, "audio", "mute-toggle", "@DEFAULT_AUDIO_SINK@",
        NULL, FALSE);
    menu_add_backend_item(
        app, "microphone-disabled-symbolic", "Mute microfono",
        "Attiva o disattiva l’ingresso predefinito", NULL,
        "audio", "mute-toggle", "@DEFAULT_AUDIO_SOURCE@", NULL,
        FALSE);
    menu_add_backend_item(
        app, "multimedia-volume-control-symbolic", "Mixer completo",
        "Applicazioni, stream, profili e porte", "PAVUCONTROL",
        "audio", "mixer", NULL, NULL, TRUE);
    menu_add_nav(app, "bluetooth-active-symbolic", "Cuffie Bluetooth",
                 "Connessione, profilo musica e microfono", NULL,
                 "bluetooth");

    menu_add_section(app, "IN RIPRODUZIONE");
    g_autofree char *player = anto_audio_player_text(snapshot);
    menu_add_shell_item(app, "media-playback-start-symbolic",
                        "In riproduzione", player, "MPRIS",
                        "$HOME/.config/anto426/media_status.sh menu", TRUE);
    GtkWidget *player_row = gtk_widget_get_last_child(app->list);
    runtime->player_row = player_row;
    runtime->player_detail =
        anto_audio_find_css_descendant(player_row, "item-subtitle");
    menu_add_backend_item(
        app, "media-skip-backward-symbolic", "Brano precedente",
        "Controlla la riproduzione in corso", NULL, "audio", "player",
        "previous", NULL, FALSE);
    menu_add_backend_item(
        app, "media-playback-start-symbolic", "Play / Pausa",
        "Controlla la riproduzione in corso", NULL, "audio", "player",
        "play-pause", NULL, FALSE);
    menu_add_backend_item(
        app, "media-skip-forward-symbolic", "Brano successivo",
        "Controlla la riproduzione in corso", NULL, "audio", "player",
        "next", NULL, FALSE);
    menu_set_footer(app,
                    "Volume e riproduzione si aggiornano automaticamente");
    g_weak_ref_set(&runtime->root, G_OBJECT(root));
    runtime->full_view = TRUE;
    anto_audio_apply_snapshot(runtime, snapshot);
}

void menu_show_audio(MenuApp *app) {
    AudioRuntime *runtime = anto_audio_runtime_get(app);
    if (!anto_audio_root_is_current(runtime))
        anto_audio_render(runtime, runtime->snapshot);
    anto_audio_start_snapshot(app);
}
