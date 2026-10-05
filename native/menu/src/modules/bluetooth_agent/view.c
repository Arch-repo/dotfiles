#include "internal.h"
#include "primitives.h"

gboolean anto_bluetooth_agent_agent_prompt(BluetoothAgent *agent, const char *method,
                               const char *device, const char *message,
                               GDBusMethodInvocation *invocation) {
    GtkWindow *window = g_weak_ref_get(&agent->window);
    if (!window || agent->stopped || agent->app->closing) {
        g_clear_object(&window);
        return FALSE;
    }
    anto_bluetooth_agent_agent_prompt_clear(agent, "Richiesta sostituita");
    agent->device = g_strdup(device);
    agent->method = g_strdup(method);
    if (invocation) agent->invocation = g_object_ref(invocation);

    agent->prompt = g_object_ref_sink(gtk_popover_new());
    gtk_popover_set_autohide(GTK_POPOVER(agent->prompt), FALSE);
    gtk_popover_set_has_arrow(GTK_POPOVER(agent->prompt), FALSE);
    gtk_widget_set_parent(agent->prompt, agent->app->panel);
    GdkRectangle center = {gtk_widget_get_width(agent->app->panel) / 2,
                           gtk_widget_get_height(agent->app->panel) / 2, 1, 1};
    gtk_popover_set_pointing_to(GTK_POPOVER(agent->prompt), &center);
    GtkWidget *content = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_size_request(content, 360, -1);
    gtk_widget_set_margin_top(content, 18);
    gtk_widget_set_margin_bottom(content, 18);
    gtk_widget_set_margin_start(content, 18);
    gtk_widget_set_margin_end(content, 18);
    gtk_popover_set_child(GTK_POPOVER(agent->prompt), content);
    GtkWidget *title = anto_ui_text("Associazione Bluetooth", "ui-summary-title", 1);
    gtk_widget_add_css_class(title, "bluetooth-device-name");
    gtk_box_append(GTK_BOX(content), title);
    g_autofree char *address = anto_bluetooth_agent_device_address(device);
    gtk_box_append(GTK_BOX(content), anto_ui_text(address, "ui-caption", 1));
    GtkWidget *text = anto_ui_text(message, "item-subtitle", 3);
    gtk_label_set_wrap(GTK_LABEL(text), TRUE);
    gtk_label_set_max_width_chars(GTK_LABEL(text), 42);
    gtk_box_append(GTK_BOX(content), text);
    gboolean input = g_strcmp0(method, "RequestPinCode") == 0 ||
                     g_strcmp0(method, "RequestPasskey") == 0;
    if (input) {
        GtkWidget *field = anto_ui_field("Codice di associazione", "", &agent->entry);
        gtk_entry_set_max_length(GTK_ENTRY(agent->entry),
            g_strcmp0(method, "RequestPinCode") == 0 ? 16 : 6);
        gtk_entry_set_input_purpose(GTK_ENTRY(agent->entry),
            g_strcmp0(method, "RequestPinCode") == 0 ? GTK_INPUT_PURPOSE_FREE_FORM : GTK_INPUT_PURPOSE_DIGITS);
        gtk_box_append(GTK_BOX(content), field);
    }
    agent->validation = anto_ui_text("", "ui-form-status", 2);
    gtk_label_set_wrap(GTK_LABEL(agent->validation), TRUE);
    gtk_box_append(GTK_BOX(content), agent->validation);
    GtkWidget *buttons = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    GtkWidget *cancel = anto_ui_action("Annulla", NULL, NULL);
    g_signal_connect(cancel, "clicked", G_CALLBACK(anto_bluetooth_agent_agent_cancel), agent);
    gtk_box_append(GTK_BOX(buttons), cancel);
    if (invocation) {
        GtkWidget *confirm = anto_ui_action(input ? "Associa" : "Conferma", NULL, "primary");
        gtk_widget_add_css_class(confirm, "suggested-action");
        g_signal_connect(confirm, "clicked", G_CALLBACK(anto_bluetooth_agent_agent_confirm), agent);
        gtk_box_append(GTK_BOX(buttons), confirm);
    }
    gtk_box_append(GTK_BOX(content), buttons);
    gtk_popover_popup(GTK_POPOVER(agent->prompt));
    if (agent->entry) gtk_widget_grab_focus(agent->entry);
    g_object_unref(window);
    return TRUE;
}
