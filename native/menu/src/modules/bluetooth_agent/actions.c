#include "internal.h"

void anto_bluetooth_agent_agent_cancel(GtkButton *button, gpointer data) {
    (void)button;
    BluetoothAgent *agent = data;
    if (agent->device && agent->bus)
        g_dbus_connection_call(agent->bus, "org.bluez", agent->device,
            "org.bluez.Device1", "CancelPairing", NULL, NULL,
            G_DBUS_CALL_FLAGS_NONE, 5000, NULL, NULL, NULL);
    anto_bluetooth_agent_agent_prompt_clear(agent, "Associazione annullata dall’utente");
}

void anto_bluetooth_agent_agent_confirm(GtkButton *button, gpointer data) {
    (void)button;
    BluetoothAgent *agent = data;
    if (!agent->invocation) return;
    GVariant *reply = NULL;
    if (g_strcmp0(agent->method, "RequestPinCode") == 0) {
        const char *pin = gtk_editable_get_text(GTK_EDITABLE(agent->entry));
        glong length = g_utf8_strlen(pin, -1);
        if (length < 1 || length > 16) {
            gtk_label_set_text(GTK_LABEL(agent->validation), "Inserisci un PIN da 1 a 16 caratteri.");
            return;
        }
        reply = g_variant_new("(s)", pin);
    } else if (g_strcmp0(agent->method, "RequestPasskey") == 0) {
        const char *pin = gtk_editable_get_text(GTK_EDITABLE(agent->entry));
        gboolean valid = *pin && strlen(pin) <= 6;
        for (const char *cursor = pin; *cursor; cursor++) valid &= g_ascii_isdigit(*cursor);
        if (!valid) {
            gtk_label_set_text(GTK_LABEL(agent->validation), "Inserisci un codice numerico da 1 a 6 cifre.");
            return;
        }
        reply = g_variant_new("(u)", (guint32)g_ascii_strtoull(pin, NULL, 10));
    }
    g_dbus_method_invocation_return_value(agent->invocation, reply);
    g_clear_object(&agent->invocation);
    anto_bluetooth_agent_agent_prompt_clear(agent, "Richiesta completata");
}
