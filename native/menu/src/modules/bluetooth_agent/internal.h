#pragma once
/* Page-private state and cross-file contract. Public API stays in menu.h. */
#include "menu.h"

#include <string.h>

#define AGENT_PATH "/com/anto426/NativeMenu/BluetoothAgent"

typedef struct {
    gint refs;
    MenuApp *app;
    GWeakRef window;
    GDBusConnection *bus;
    guint watcher;
    guint registration;
    gboolean stopped;
    gboolean ready;
    char *owner;
    GDBusMethodInvocation *invocation;
    GtkWidget *prompt;
    GtkWidget *entry;
    GtkWidget *validation;
    char *device;
    char *method;
} BluetoothAgent;

extern const char anto_bluetooth_agent_agent_xml[];
extern const GDBusInterfaceVTable anto_bluetooth_agent_agent_vtable;

BluetoothAgent *anto_bluetooth_agent_agent_ref(BluetoothAgent *agent);
void anto_bluetooth_agent_agent_unref(gpointer data);
void anto_bluetooth_agent_agent_prompt_clear(BluetoothAgent *agent, const char *reason);
void anto_bluetooth_agent_agent_cancel(GtkButton *button, gpointer data);
void anto_bluetooth_agent_agent_confirm(GtkButton *button, gpointer data);
char *anto_bluetooth_agent_device_address(const char *path);
gboolean anto_bluetooth_agent_agent_prompt(BluetoothAgent *agent, const char *method,
                               const char *device, const char *message,
                               GDBusMethodInvocation *invocation);
void anto_bluetooth_agent_agent_method(GDBusConnection *connection, const char *sender,
                          const char *path, const char *interface,
                          const char *method, GVariant *parameters,
                          GDBusMethodInvocation *invocation, gpointer data);
void anto_bluetooth_agent_agent_default_finished(GObject *object, GAsyncResult *result, gpointer data);
void anto_bluetooth_agent_agent_registered(GObject *object, GAsyncResult *result, gpointer data);
void anto_bluetooth_agent_bluez_appeared(GDBusConnection *connection, const char *name,
                            const char *owner, gpointer data);
void anto_bluetooth_agent_bluez_vanished(GDBusConnection *connection, const char *name, gpointer data);
gboolean anto_bluetooth_agent_agent_window_closed(GtkWindow *window, gpointer data);
void menu_bluetooth_agent_start(MenuApp *app);
void menu_bluetooth_agent_dismiss(MenuApp *app);
