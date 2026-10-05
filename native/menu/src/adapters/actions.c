#include "menu.h"

typedef struct {
    char *command;
    gboolean close_after;
} ShellItem;

static void shell_item_free(gpointer data) {
    ShellItem *item = data;
    if (!item) return;
    g_free(item->command);
    g_free(item);
}

static void run_shell_item(MenuApp *app, gpointer data) {
    ShellItem *item = data;
    menu_spawn_shell(app, item->command, item->close_after);
}

void menu_add_shell_item(MenuApp *app, const char *icon, const char *title,
                         const char *subtitle, const char *badge,
                         const char *command, gboolean close_after) {
    ShellItem *item = g_new0(ShellItem, 1);
    item->command = g_strdup(command);
    item->close_after = close_after;
    menu_add_item(app, icon, title, subtitle, badge,
                  run_shell_item, item, shell_item_free);
}

void menu_add_shell_tile(MenuApp *app, const char *icon, const char *title,
                         const char *subtitle, const char *badge,
                         const char *command, gboolean close_after) {
    ShellItem *item = g_new0(ShellItem, 1);
    item->command = g_strdup(command);
    item->close_after = close_after;
    menu_add_tile(app, icon, title, subtitle, badge,
                  run_shell_item, item, shell_item_free);
}
