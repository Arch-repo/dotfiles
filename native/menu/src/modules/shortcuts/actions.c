#include "internal.h"

void anto_shortcuts_open_config(MenuApp *app, gpointer data) {
    (void)data;
    g_autofree char *path = menu_config_path("hypr/conf/keybinding.conf");
    const char *argv[] = {"xdg-open", path, NULL};
    menu_spawn(app, argv, TRUE);
}
