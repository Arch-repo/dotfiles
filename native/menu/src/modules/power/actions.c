#include "internal.h"

void anto_power_action_free(gpointer data) {
    PowerAction *action = data;
    if (!action) return;
    g_free(action->title);
    g_free(action->operation);
    g_free(action);
}

void anto_power_confirm_execute(MenuApp *app, gpointer data) {
    PowerAction *action = data;
    menu_spawn_backend(app, "session", action->operation,
                       NULL, NULL, TRUE);
}

void anto_power_cancel_confirm(MenuApp *app, gpointer data) {
    (void)data;
    menu_back(app);
}

void anto_power_show_confirm(MenuApp *app, gpointer data) {
    PowerAction *action = data;
    g_autofree char *title = g_strdup(action->title);
    g_autofree char *operation = g_strdup(action->operation);
    g_ptr_array_add(app->history, g_strdup("power"));
    g_free(app->current_page);
    app->current_page = g_strdup("power-confirm");
    g_autofree char *subtitle = g_strdup_printf("Confermi: %s?", title);
    menu_page_begin(app, "dialog-warning-symbolic", "Conferma azione", subtitle,
                    "Digita conferma…");
    PowerAction *copy = g_new0(PowerAction, 1);
    copy->title = g_strdup(title);
    copy->operation = g_strdup(operation);
    menu_add_item(app, "dialog-ok-symbolic", "Sì, conferma",
                  "L’azione verrà eseguita immediatamente", "CONFERMA",
                  anto_power_confirm_execute, copy, anto_power_action_free);
    menu_add_item(app, "go-previous-symbolic", "Annulla",
                  "Torna al menu di alimentazione", NULL,
                  anto_power_cancel_confirm, NULL, NULL);
}

void anto_power_add_confirm(MenuApp *app, const char *icon, const char *title,
                        const char *subtitle, const char *operation) {
    PowerAction *action = g_new0(PowerAction, 1);
    action->title = g_strdup(title);
    action->operation = g_strdup(operation);
    menu_add_tile(app, icon, title, subtitle, NULL,
                  anto_power_show_confirm, action, anto_power_action_free);
}
