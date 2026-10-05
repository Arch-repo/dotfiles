#pragma once
/* Page-private state and cross-file contract. Public API stays in menu.h. */
#include "menu.h"

typedef struct {
    char *title;
    char *operation;
} PowerAction;



void anto_power_action_free(gpointer data);
void anto_power_confirm_execute(MenuApp *app, gpointer data);
void anto_power_cancel_confirm(MenuApp *app, gpointer data);
void anto_power_show_confirm(MenuApp *app, gpointer data);
void anto_power_add_confirm(MenuApp *app, const char *icon, const char *title,
                        const char *subtitle, const char *operation);
char *anto_power_session_uptime(void);
void menu_show_power(MenuApp *app);
