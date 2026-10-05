#include "internal.h"

void menu_show_record(MenuApp *app) {
    g_autofree char *backend = menu_backend_path();
    const char *status_argv[] = {
        backend, "record", "status", NULL,
    };
    char *status_output = NULL;
    char *status_error = NULL;
    gboolean status_ok = menu_run_with_input(
        status_argv, NULL, &status_output, &status_error);
    g_autofree char *status =
        status_ok && status_output ? status_output : g_strdup("");
    g_free(status_error);
    g_strstrip(status);
    menu_page_begin(app, "media-record-symbolic", "Registrazione",
                    *status ? status : "Pronto", "Cerca modalità di registrazione…");
    menu_set_layout(app, MENU_LAYOUT_GRID);
    menu_set_grid_columns(app, 3);
    menu_add_backend_tile(
        app, "media-record-symbolic", "Registra area",
        "Seleziona un rettangolo sullo schermo", NULL,
        "record", "area", NULL, NULL, TRUE);
    menu_add_backend_tile(
        app, "focus-windows-symbolic", "Registra finestra",
        "Segue la geometria della finestra attiva", NULL,
        "record", "window", NULL, NULL, TRUE);
    menu_add_backend_tile(
        app, "video-display-symbolic", "Registra monitor",
        "Video del monitor attivo senza audio", NULL,
        "record", "monitor", NULL, NULL, TRUE);
    menu_add_backend_tile(
        app, "audio-speakers-symbolic", "Monitor con audio",
        "Video più stream audio PipeWire", NULL,
        "record", "monitor-audio", NULL, NULL, TRUE);
    menu_add_backend_tile(
        app, "media-playback-stop-symbolic", "Ferma registrazione",
        "Finalizza e salva il video corrente", "STOP",
        "record", "stop", NULL, NULL, TRUE);
    menu_add_backend_tile(
        app, "folder-videos-symbolic", "Apri registrazioni",
        "Mostra i video salvati", NULL,
        "record", "open", NULL, NULL, TRUE);
}
