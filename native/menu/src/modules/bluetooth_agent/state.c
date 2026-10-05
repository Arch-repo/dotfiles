#include "internal.h"

const char anto_bluetooth_agent_agent_xml[] =
    "<node><interface name='org.bluez.Agent1'>"
    "<method name='Release'/>"
    "<method name='RequestPinCode'><arg type='o' direction='in'/><arg type='s' direction='out'/></method>"
    "<method name='DisplayPinCode'><arg type='o' direction='in'/><arg type='s' direction='in'/></method>"
    "<method name='RequestPasskey'><arg type='o' direction='in'/><arg type='u' direction='out'/></method>"
    "<method name='DisplayPasskey'><arg type='o' direction='in'/><arg type='u' direction='in'/><arg type='q' direction='in'/></method>"
    "<method name='RequestConfirmation'><arg type='o' direction='in'/><arg type='u' direction='in'/></method>"
    "<method name='RequestAuthorization'><arg type='o' direction='in'/></method>"
    "<method name='AuthorizeService'><arg type='o' direction='in'/><arg type='s' direction='in'/></method>"
    "<method name='Cancel'/>"
    "</interface></node>";
const GDBusInterfaceVTable anto_bluetooth_agent_agent_vtable = {.method_call = anto_bluetooth_agent_agent_method};

BluetoothAgent *anto_bluetooth_agent_agent_ref(BluetoothAgent *agent) {
    g_atomic_int_inc(&agent->refs);
    return agent;
}

void anto_bluetooth_agent_agent_unref(gpointer data) {
    BluetoothAgent *agent = data;
    if (!agent || !g_atomic_int_dec_and_test(&agent->refs)) return;
    g_weak_ref_clear(&agent->window);
    g_clear_object(&agent->bus);
    g_free(agent->owner);
    g_free(agent->device);
    g_free(agent->method);
    g_free(agent);
}

void anto_bluetooth_agent_agent_prompt_clear(BluetoothAgent *agent, const char *reason) {
    if (agent->invocation) {
        g_dbus_method_invocation_return_dbus_error(
            agent->invocation, "org.bluez.Error.Canceled", reason);
        g_clear_object(&agent->invocation);
    }
    if (agent->prompt) {
        gtk_popover_popdown(GTK_POPOVER(agent->prompt));
        if (gtk_widget_get_parent(agent->prompt)) gtk_widget_unparent(agent->prompt);
        g_clear_object(&agent->prompt);
    }
    agent->entry = NULL;
    agent->validation = NULL;
    g_clear_pointer(&agent->device, g_free);
    g_clear_pointer(&agent->method, g_free);
}

char *anto_bluetooth_agent_device_address(const char *path) {
    const char *last = strrchr(path, '/');
    const char *token = last && g_str_has_prefix(last, "/dev_") ? last + 5 : path;
    char *address = g_strdup(token);
    g_strdelimit(address, "_", ':');
    return address;
}

void anto_bluetooth_agent_agent_method(GDBusConnection *connection, const char *sender,
                          const char *path, const char *interface,
                          const char *method, GVariant *parameters,
                          GDBusMethodInvocation *invocation, gpointer data) {
    (void)connection;
    (void)path;
    (void)interface;
    BluetoothAgent *agent = data;
    if (g_strcmp0(sender, agent->owner) != 0 || agent->stopped) {
        g_dbus_method_invocation_return_dbus_error(invocation,
            "org.bluez.Error.Rejected", "Richiesta non proveniente da BlueZ");
        return;
    }
    if (g_strcmp0(method, "Release") == 0 || g_strcmp0(method, "Cancel") == 0) {
        anto_bluetooth_agent_agent_prompt_clear(agent, "Richiesta annullata da BlueZ");
        if (g_strcmp0(method, "Release") == 0) agent->ready = FALSE;
        g_dbus_method_invocation_return_value(invocation, NULL);
        return;
    }
    const char *device;
    g_variant_get_child(parameters, 0, "&o", &device);
    g_autofree char *message = NULL;
    gboolean display = FALSE;
    if (g_strcmp0(method, "RequestConfirmation") == 0) {
        guint32 passkey;
        g_variant_get_child(parameters, 1, "u", &passkey);
        message = g_strdup_printf("Conferma che sul dispositivo compaia il codice %06u.", passkey);
    } else if (g_strcmp0(method, "DisplayPasskey") == 0) {
        guint32 passkey;
        guint16 entered;
        g_variant_get_child(parameters, 1, "u", &passkey);
        g_variant_get_child(parameters, 2, "q", &entered);
        message = g_strdup_printf("Digita %06u sul dispositivo e premi Invio. Cifre inserite: %u/6.", passkey, entered);
        display = TRUE;
    } else if (g_strcmp0(method, "DisplayPinCode") == 0) {
        const char *pin;
        g_variant_get_child(parameters, 1, "&s", &pin);
        message = g_strdup_printf("Digita il PIN %s sul dispositivo e premi Invio.", pin);
        display = TRUE;
    } else if (g_strcmp0(method, "RequestPinCode") == 0 ||
               g_strcmp0(method, "RequestPasskey") == 0) {
        message = g_strdup("Inserisci il codice richiesto dal dispositivo.");
    } else if (g_strcmp0(method, "AuthorizeService") == 0) {
        const char *uuid;
        g_variant_get_child(parameters, 1, "&s", &uuid);
        message = g_strdup_printf("Consenti la connessione al servizio %s di questo dispositivo?", uuid);
    } else {
        message = g_strdup("Consenti l’associazione con questo dispositivo?");
    }
    if (!anto_bluetooth_agent_agent_prompt(agent, method, device, message, display ? NULL : invocation)) {
        g_dbus_method_invocation_return_dbus_error(invocation,
            "org.bluez.Error.Rejected", "Menu Bluetooth chiuso");
    } else if (display) {
        g_dbus_method_invocation_return_value(invocation, NULL);
    }
}

void anto_bluetooth_agent_agent_default_finished(GObject *object, GAsyncResult *result, gpointer data) {
    BluetoothAgent *agent = data;
    g_autoptr(GError) error = NULL;
    g_autoptr(GVariant) reply = g_dbus_connection_call_finish(G_DBUS_CONNECTION(object), result, &error);
    agent->ready = reply && !agent->stopped;
    if (error && !agent->stopped)
        menu_notify("Bluetooth", "Impossibile attivare la conferma associazioni nel menu.");
    anto_bluetooth_agent_agent_unref(agent);
}

void anto_bluetooth_agent_agent_registered(GObject *object, GAsyncResult *result, gpointer data) {
    BluetoothAgent *agent = data;
    g_autoptr(GError) error = NULL;
    g_autoptr(GVariant) reply = g_dbus_connection_call_finish(G_DBUS_CONNECTION(object), result, &error);
    if (reply && !agent->stopped)
        g_dbus_connection_call(agent->bus, "org.bluez", "/org/bluez",
            "org.bluez.AgentManager1", "RequestDefaultAgent", g_variant_new("(o)", AGENT_PATH),
            NULL, G_DBUS_CALL_FLAGS_NONE, 5000, NULL, anto_bluetooth_agent_agent_default_finished, anto_bluetooth_agent_agent_ref(agent));
    else if (error && !agent->stopped)
        menu_notify("Bluetooth", "Impossibile preparare la conferma delle associazioni.");
    anto_bluetooth_agent_agent_unref(agent);
}

void anto_bluetooth_agent_bluez_appeared(GDBusConnection *connection, const char *name,
                            const char *owner, gpointer data) {
    (void)name;
    BluetoothAgent *agent = data;
    if (agent->stopped) return;
    g_set_object(&agent->bus, connection);
    g_free(agent->owner);
    agent->owner = g_strdup(owner);
    if (!agent->registration) {
        g_autoptr(GDBusNodeInfo) info = g_dbus_node_info_new_for_xml(anto_bluetooth_agent_agent_xml, NULL);
        agent->registration = g_dbus_connection_register_object(connection,
            AGENT_PATH, info->interfaces[0], &anto_bluetooth_agent_agent_vtable,
            anto_bluetooth_agent_agent_ref(agent), anto_bluetooth_agent_agent_unref, NULL);
    }
    if (agent->registration)
        g_dbus_connection_call(connection, "org.bluez", "/org/bluez",
            "org.bluez.AgentManager1", "RegisterAgent", g_variant_new("(os)", AGENT_PATH, "KeyboardDisplay"),
            NULL, G_DBUS_CALL_FLAGS_NONE, 5000, NULL, anto_bluetooth_agent_agent_registered, anto_bluetooth_agent_agent_ref(agent));
    menu_bluetooth_live_event(agent->app);
}

void anto_bluetooth_agent_bluez_vanished(GDBusConnection *connection, const char *name, gpointer data) {
    (void)connection;
    (void)name;
    BluetoothAgent *agent = data;
    agent->ready = FALSE;
    anto_bluetooth_agent_agent_prompt_clear(agent, "Servizio Bluetooth non disponibile");
    g_clear_pointer(&agent->owner, g_free);
    if (!agent->stopped) menu_bluetooth_live_event(agent->app);
}

gboolean anto_bluetooth_agent_agent_window_closed(GtkWindow *window, gpointer data) {
    (void)window;
    BluetoothAgent *agent = data;
    agent->stopped = TRUE;
    anto_bluetooth_agent_agent_prompt_clear(agent, "Menu Bluetooth chiuso");
    if (agent->bus && agent->ready)
        g_dbus_connection_call(agent->bus, "org.bluez", "/org/bluez",
            "org.bluez.AgentManager1", "UnregisterAgent", g_variant_new("(o)", AGENT_PATH),
            NULL, G_DBUS_CALL_FLAGS_NONE, 5000, NULL, NULL, NULL);
    agent->ready = FALSE;
    if (agent->watcher) {
        g_bus_unwatch_name(agent->watcher);
        agent->watcher = 0;
    }
    if (agent->registration) {
        g_dbus_connection_unregister_object(agent->bus, agent->registration);
        agent->registration = 0;
    }
    return FALSE;
}

void menu_bluetooth_agent_start(MenuApp *app) {
    if (!app || !app->window || app->closing) return;
    if (g_object_get_data(G_OBJECT(app->window), "anto-bluetooth-agent")) return;
    BluetoothAgent *agent = g_new0(BluetoothAgent, 1);
    agent->refs = 1;
    agent->app = app;
    g_weak_ref_init(&agent->window, G_OBJECT(app->window));
    g_object_set_data_full(G_OBJECT(app->window), "anto-bluetooth-agent", agent, anto_bluetooth_agent_agent_unref);
    g_signal_connect(app->window, "close-request", G_CALLBACK(anto_bluetooth_agent_agent_window_closed), agent);
    agent->watcher = g_bus_watch_name(G_BUS_TYPE_SYSTEM, "org.bluez", G_BUS_NAME_WATCHER_FLAGS_NONE,
        anto_bluetooth_agent_bluez_appeared, anto_bluetooth_agent_bluez_vanished, anto_bluetooth_agent_agent_ref(agent), anto_bluetooth_agent_agent_unref);
}

void menu_bluetooth_agent_dismiss(MenuApp *app) {
    if (!app || !app->window) return;
    BluetoothAgent *agent = g_object_get_data(G_OBJECT(app->window), "anto-bluetooth-agent");
    if (agent) anto_bluetooth_agent_agent_prompt_clear(agent, "Associazione completata");
}
