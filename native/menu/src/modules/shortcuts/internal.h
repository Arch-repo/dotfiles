#pragma once
/* Page-private state and cross-file contract. Public API stays in menu.h. */
#include "menu.h"

typedef struct {
    const char *section;
    const char *keys;
    const char *description;
} Shortcut;

extern const Shortcut anto_shortcuts_shortcuts[29];

void anto_shortcuts_open_config(MenuApp *app, gpointer data);
void menu_show_shortcuts(MenuApp *app);
