#include "internal.h"

void menu_show_power(MenuApp *app) {
    g_autofree char *uptime = anto_power_session_uptime();
    g_autofree char *subtitle =
        g_strdup_printf("Sessione attiva · %s", uptime);
    menu_page_begin(app, "system-shutdown-symbolic", "Alimentazione e sessione",
                    subtitle, "Cerca un’azione…");
    menu_set_layout(app, MENU_LAYOUT_GRID);
    menu_set_grid_columns(app, 3);
    menu_add_backend_tile(
        app, "system-lock-screen-symbolic", "Blocca",
        "Mantiene aperta la sessione", NULL, "session", "lock",
        NULL, NULL, TRUE);
    menu_add_backend_tile(
        app, "media-playback-pause-symbolic", "Sospendi",
        "Salva energia e riprende rapidamente", NULL,
        "session", "suspend", NULL, NULL, TRUE);
    anto_power_add_confirm(app, "system-log-out-symbolic", "Esci da Hyprland",
                "Chiude la sessione grafica corrente",
                "logout");
    anto_power_add_confirm(app, "system-reboot-symbolic", "Riavvia",
                "Riavvia completamente il computer",
                "reboot");
    anto_power_add_confirm(app, "system-shutdown-symbolic", "Spegni",
                "Arresta il computer",
                "poweroff");
}
