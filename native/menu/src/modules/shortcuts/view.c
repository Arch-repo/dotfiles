#include "internal.h"

void menu_show_shortcuts(MenuApp *app) {
    menu_page_begin(app, "preferences-desktop-keyboard-shortcuts-symbolic",
                    "Scorciatoie", "La mappa essenziale della tua sessione Hyprland",
                    "Cerca un tasto o un’azione…");
    for (guint i = 0; i < G_N_ELEMENTS(anto_shortcuts_shortcuts); i++) {
        if (anto_shortcuts_shortcuts[i].section) menu_add_section(app, anto_shortcuts_shortcuts[i].section);
        menu_add_item(app, "input-keyboard-symbolic", anto_shortcuts_shortcuts[i].keys,
                      anto_shortcuts_shortcuts[i].description, NULL, NULL, NULL, NULL);
    }
    menu_add_section(app, "CONFIGURAZIONE");
    menu_add_item(app, "text-x-generic-symbolic", "Apri keybinding.conf",
                  "Modifica le scorciatoie alla fonte", NULL,
                  anto_shortcuts_open_config, NULL, NULL);
}
