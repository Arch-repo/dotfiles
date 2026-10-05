#include "internal.h"

void anto_bluetooth_action_free(gpointer data, GClosure *closure) {
    (void)closure;
    BluetoothAction *action = data;
    if (!action) return;
    g_free(action->operation);
    g_free(action->argument);
    g_free(action->value);
    g_free(action);
}

void anto_bluetooth_action_clicked(GtkButton *button, gpointer data) {
    BluetoothAction *action = data;
    anto_bluetooth_operation_start(action->app, GTK_WIDGET(button), action->operation,
                              action->argument, action->value);
}

void anto_bluetooth_manager_clicked(GtkButton *button, gpointer data) {
    (void)button;
    MenuApp *app = data;
    const char *argv[] = {"blueman-manager", NULL};
    menu_spawn(app, argv, TRUE);
}

void anto_bluetooth_switch_action_free(gpointer data, GClosure *closure) {
    (void)closure;
    BluetoothSwitchAction *action = data;
    if (!action) return;
    g_free(action->operation);
    g_free(action);
}

void anto_bluetooth_switch_changed(GObject *object, GParamSpec *spec, gpointer data) {
    (void)spec;
    if (g_object_get_data(object, "bluetooth-syncing")) return;
    BluetoothSwitchAction *action = data;
    gboolean active = gtk_switch_get_active(GTK_SWITCH(object));
    anto_bluetooth_operation_start(action->app, GTK_WIDGET(object), action->operation,
                              active ? "on" : "off", NULL);
}
