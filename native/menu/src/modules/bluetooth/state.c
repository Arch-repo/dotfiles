#include "internal.h"

char *anto_bluetooth_action_path(void) {
    return menu_backend_path();
}

gboolean anto_bluetooth_text_is_true(const char *text) {
    return g_strcmp0(text, "yes") == 0;
}

const char *anto_bluetooth_present_text(const char *text) {
    return text && *text && g_strcmp0(text, "-") != 0 ? text : NULL;
}

guint anto_bluetooth_parse_count(const char *text) {
    char *end = NULL;
    unsigned long value = g_ascii_strtoull(text ? text : "0", &end, 10);
    return end && end != text ? (guint)MIN(value, G_MAXUINT) : 0;
}

int anto_bluetooth_parse_number(const char *text, int fallback) {
    char *end = NULL;
    long value = strtol(text ? text : "", &end, 10);
    return end && end != text ? (int)CLAMP(value, G_MININT, G_MAXINT) : fallback;
}

gboolean anto_bluetooth_run_bluetooth(const char *operation, const char *argument,
                              const char *value, char **output, char **error) {
    g_autofree char *script = anto_bluetooth_action_path();
    const char *argv[] = {
        script, "bluetooth", operation, argument, value, NULL,
    };
    return menu_run_with_input(argv, NULL, output, error);
}

char *anto_bluetooth_read_bluetooth(const char *operation, const char *argument) {
    char *output = NULL;
    char *error = NULL;
    if (!anto_bluetooth_run_bluetooth(operation, argument, NULL, &output, &error)) {
        g_free(output);
        g_free(error);
        return g_strdup("");
    }
    g_free(error);
    return output ? output : g_strdup("");
}

void anto_bluetooth_status_clear(BluetoothStatus *status) {
    g_free(status->address);
    g_free(status->alias);
}

BluetoothStatus anto_bluetooth_status_parse(const char *output) {
    BluetoothStatus status = {0};
    g_auto(GStrv) lines = g_strsplit(output, "\n", 2);
    g_auto(GStrv) fields = g_strsplit(lines[0], "\t", -1);
    if (g_strv_length(fields) < 11 || g_strcmp0(fields[0], "STATUS") != 0)
        return status;

    status.available = anto_bluetooth_text_is_true(fields[1]);
    status.address = g_strdup(anto_bluetooth_present_text(fields[2]));
    status.alias = g_strdup(anto_bluetooth_present_text(fields[3]));
    status.powered = anto_bluetooth_text_is_true(fields[4]);
    status.pairable = anto_bluetooth_text_is_true(fields[5]);
    status.discoverable = anto_bluetooth_text_is_true(fields[6]);
    status.discovering = anto_bluetooth_text_is_true(fields[7]);
    status.paired_count = anto_bluetooth_parse_count(fields[8]);
    status.connected_count = anto_bluetooth_parse_count(fields[9]);
    status.seen_count = anto_bluetooth_parse_count(fields[10]);
    return status;
}

void anto_bluetooth_controller_free(gpointer data) {
    BluetoothController *controller = data;
    if (!controller) return;
    g_free(controller->address);
    g_free(controller->alias);
    g_free(controller->name);
    g_free(controller);
}

GPtrArray *anto_bluetooth_controllers_parse(const char *output) {
    GPtrArray *controllers = g_ptr_array_new_with_free_func(anto_bluetooth_controller_free);
    g_auto(GStrv) lines = g_strsplit(output, "\n", -1);
    for (guint index = 0; lines[index]; index++) {
        g_auto(GStrv) fields = g_strsplit(lines[index], "\t", -1);
        if (g_strv_length(fields) < 9 || g_strcmp0(fields[0], "CONTROLLER") != 0)
            continue;
        BluetoothController *controller = g_new0(BluetoothController, 1);
        controller->address = g_strdup(anto_bluetooth_present_text(fields[1]));
        controller->alias = g_strdup(anto_bluetooth_present_text(fields[2]));
        controller->name = g_strdup(anto_bluetooth_present_text(fields[3]));
        controller->powered = anto_bluetooth_text_is_true(fields[4]);
        controller->pairable = anto_bluetooth_text_is_true(fields[5]);
        controller->discoverable = anto_bluetooth_text_is_true(fields[6]);
        controller->discovering = anto_bluetooth_text_is_true(fields[7]);
        controller->is_default = anto_bluetooth_text_is_true(fields[8]);
        g_ptr_array_add(controllers, controller);
    }
    return controllers;
}

void anto_bluetooth_device_free(gpointer data) {
    BluetoothDevice *device = data;
    if (!device) return;
    g_free(device->address);
    g_free(device->name);
    g_free(device->alias);
    g_free(device->icon);
    g_free(device);
}

GPtrArray *anto_bluetooth_devices_parse(const char *output) {
    GPtrArray *devices = g_ptr_array_new_with_free_func(anto_bluetooth_device_free);
    g_auto(GStrv) lines = g_strsplit(output, "\n", -1);
    for (guint index = 0; lines[index]; index++) {
        g_auto(GStrv) fields = g_strsplit(lines[index], "\t", -1);
        if (g_strv_length(fields) < 11 || g_strcmp0(fields[0], "DEVICE") != 0)
            continue;
        BluetoothDevice *device = g_new0(BluetoothDevice, 1);
        device->address = g_strdup(anto_bluetooth_present_text(fields[1]));
        device->name = g_strdup(anto_bluetooth_present_text(fields[2]));
        device->alias = g_strdup(anto_bluetooth_present_text(fields[3]));
        device->icon = g_strdup(anto_bluetooth_present_text(fields[4]));
        device->paired = anto_bluetooth_text_is_true(fields[5]);
        device->trusted = anto_bluetooth_text_is_true(fields[6]);
        device->blocked = anto_bluetooth_text_is_true(fields[7]);
        device->connected = anto_bluetooth_text_is_true(fields[8]);
        device->battery = anto_bluetooth_parse_number(fields[9], -1);
        device->rssi = anto_bluetooth_parse_number(fields[10], -1);
        if (device->address) g_ptr_array_add(devices, device);
        else anto_bluetooth_device_free(device);
    }
    return devices;
}

void anto_bluetooth_audio_profile_free(gpointer data) {
    BluetoothAudioProfile *profile = data;
    if (!profile) return;
    g_free(profile->profile);
    g_free(profile->description);
    g_free(profile);
}

GPtrArray *anto_bluetooth_audio_profiles_read(const char *address) {
    GPtrArray *profiles = g_ptr_array_new_with_free_func(anto_bluetooth_audio_profile_free);
    g_autofree char *output = anto_bluetooth_read_bluetooth("audio-profiles", address);
    g_auto(GStrv) lines = g_strsplit(output, "\n", -1);
    for (guint index = 0; lines[index]; index++) {
        g_auto(GStrv) fields = g_strsplit(lines[index], "\t", -1);
        if (g_strv_length(fields) < 6 || g_strcmp0(fields[0], "AUDIO_PROFILE") != 0)
            continue;
        BluetoothAudioProfile *profile = g_new0(BluetoothAudioProfile, 1);
        profile->profile = g_strdup(anto_bluetooth_present_text(fields[2]));
        profile->available = g_strcmp0(fields[3], "no") != 0;
        profile->active = anto_bluetooth_text_is_true(fields[4]);
        profile->description = g_strdup(anto_bluetooth_present_text(fields[5]));
        if (profile->profile) g_ptr_array_add(profiles, profile);
        else anto_bluetooth_audio_profile_free(profile);
    }
    return profiles;
}

void anto_bluetooth_snapshot_free(gpointer data) {
    BluetoothSnapshot *snapshot = data;
    if (!snapshot) return;
    anto_bluetooth_status_clear(&snapshot->status);
    g_clear_pointer(&snapshot->controllers, g_ptr_array_unref);
    g_clear_pointer(&snapshot->devices, g_ptr_array_unref);
    g_clear_pointer(&snapshot->audio_profiles, g_hash_table_unref);
    g_free(snapshot);
}

void anto_bluetooth_snapshot_load(GTask *task, gpointer source,
                                    gpointer task_data, GCancellable *cancellable) {
    (void)source;
    (void)task_data;
    char *output = NULL;
    char *error_text = NULL;
    gboolean loaded = anto_bluetooth_run_bluetooth("snapshot", NULL, NULL, &output, &error_text);
    if (!loaded || !output || !g_str_has_prefix(output, "STATUS\t")) {
        const char *message = error_text;
        g_auto(GStrv) fields = error_text ? g_strsplit(error_text, "\t", 3) : NULL;
        if (fields && g_strv_length(fields) == 3 && g_strcmp0(fields[0], "ERROR") == 0)
            message = g_strstrip(fields[2]);
        g_task_return_new_error(task, G_IO_ERROR, G_IO_ERROR_FAILED,
                               "%s", message && *message ? message :
                               "Impossibile leggere lo stato Bluetooth");
        g_free(output);
        g_free(error_text);
        return;
    }
    g_free(error_text);
    BluetoothSnapshot *snapshot = g_new0(BluetoothSnapshot, 1);
    snapshot->status = anto_bluetooth_status_parse(output);
    snapshot->controllers = anto_bluetooth_controllers_parse(output);
    snapshot->devices = anto_bluetooth_devices_parse(output);
    g_free(output);
    if (g_task_return_error_if_cancelled(task)) {
        anto_bluetooth_snapshot_free(snapshot);
        return;
    }

    snapshot->audio_profiles = g_hash_table_new_full(
        g_str_hash, g_str_equal, g_free, (GDestroyNotify)g_ptr_array_unref);
    for (guint index = 0; index < snapshot->devices->len; index++) {
        BluetoothDevice *device = g_ptr_array_index(snapshot->devices, index);
        if (!device->connected || !device->address) continue;
        if (g_cancellable_is_cancelled(cancellable)) {
            g_task_return_new_error(task, G_IO_ERROR, G_IO_ERROR_CANCELLED,
                                    "Aggiornamento Bluetooth annullato");
            anto_bluetooth_snapshot_free(snapshot);
            return;
        }
        GPtrArray *profiles = anto_bluetooth_audio_profiles_read(device->address);
        g_hash_table_insert(snapshot->audio_profiles, g_strdup(device->address),
                            profiles);
    }
    g_task_return_pointer(task, snapshot, anto_bluetooth_snapshot_free);
}

void anto_bluetooth_pending_free(BluetoothPending *pending) {
    if (!pending) return;
    g_weak_ref_clear(&pending->window);
    g_weak_ref_clear(&pending->source);
    g_weak_ref_clear(&pending->scope);
    g_clear_object(&pending->process);
    g_free(pending);
}

void anto_bluetooth_operation_finished(GObject *object, GAsyncResult *result,
                                         gpointer data) {
    (void)object;
    BluetoothPending *pending = data;
    char *output = NULL;
    char *error_text = NULL;
    g_autoptr(GError) error = NULL;
    gboolean ok = g_subprocess_communicate_utf8_finish(
        pending->process, result, &output, &error_text, &error);
    ok = ok && g_subprocess_get_successful(pending->process);

    GtkWidget *source = g_weak_ref_get(&pending->source);
    if (source) {
        g_object_set_data(G_OBJECT(source),
                          "bluetooth-operation-pending", NULL);
        gtk_widget_set_sensitive(source, TRUE);
    }
    g_clear_object(&source);
    GtkWidget *scope = g_weak_ref_get(&pending->scope);
    if (scope)
        g_object_set_data(G_OBJECT(scope),
                          "bluetooth-operation-pending", NULL);
    if (scope) gtk_widget_set_sensitive(scope, TRUE);
    g_clear_object(&scope);

    GtkWindow *window = g_weak_ref_get(&pending->window);
    if (window) {
        if (!ok) {
            if (error_text) g_strstrip(error_text);
            const char *message = error_text;
            g_auto(GStrv) fields = error_text ? g_strsplit(error_text, "\t", 3) : NULL;
            if (fields && g_strv_length(fields) == 3 && g_strcmp0(fields[0], "ERROR") == 0)
                message = g_strstrip(fields[2]);
            menu_notify("Bluetooth", message && *message ? message :
                        error ? error->message : "BlueZ non ha completato l’operazione");
        }
        if (g_strcmp0(pending->app->current_page, "bluetooth") == 0)
            menu_bluetooth_live_event(pending->app);
        if (pending->pairing) menu_bluetooth_agent_dismiss(pending->app);
    }
    g_clear_object(&window);
    g_free(output);
    g_free(error_text);
    anto_bluetooth_pending_free(pending);
}

void anto_bluetooth_operation_start(MenuApp *app, GtkWidget *source,
                                      const char *operation, const char *argument,
                                      const char *value) {
    GtkWidget *scope = source;
    while (scope &&
           !gtk_widget_has_css_class(scope, "bluetooth-action-bar"))
        scope = gtk_widget_get_parent(scope);
    if (g_object_get_data(G_OBJECT(source),
                          "bluetooth-operation-pending") ||
        (scope && g_object_get_data(
                      G_OBJECT(scope), "bluetooth-operation-pending")))
        return;
    g_autofree char *script = anto_bluetooth_action_path();
    const char *argv[] = {
        script, "bluetooth", operation, argument, value, NULL,
    };
    g_autoptr(GError) error = NULL;
    GSubprocess *process = g_subprocess_newv(
        argv, G_SUBPROCESS_FLAGS_STDOUT_PIPE | G_SUBPROCESS_FLAGS_STDERR_PIPE,
        &error);
    if (!process) {
        menu_notify("Bluetooth", error ? error->message :
                    "Impossibile avviare il controllo Bluetooth");
        return;
    }

    g_object_set_data(G_OBJECT(source), "bluetooth-operation-pending",
                      GINT_TO_POINTER(1));
    gtk_widget_set_sensitive(source, FALSE);
    if (scope) {
        g_object_set_data(G_OBJECT(scope), "bluetooth-operation-pending",
                          GINT_TO_POINTER(1));
        gtk_widget_set_sensitive(scope, FALSE);
    }
    BluetoothPending *pending = g_new0(BluetoothPending, 1);
    pending->app = app;
    pending->process = process;
    pending->pairing = g_strcmp0(operation, "pair") == 0;
    g_weak_ref_init(&pending->window, G_OBJECT(app->window));
    g_weak_ref_init(&pending->source, G_OBJECT(source));
    g_weak_ref_init(&pending->scope,
                    scope ? G_OBJECT(scope) : NULL);
    g_subprocess_communicate_utf8_async(process, NULL, NULL,
                                        anto_bluetooth_operation_finished, pending);
}

BluetoothAction *anto_bluetooth_action_new(MenuApp *app, const char *operation,
                                             const char *argument, const char *value) {
    BluetoothAction *action = g_new0(BluetoothAction, 1);
    action->app = app;
    action->operation = g_strdup(operation);
    action->argument = g_strdup(argument);
    action->value = g_strdup(value);
    return action;
}

void anto_bluetooth_state_chip_update(GtkWidget *chip, const char *text,
                              const char *state) {
    static const char *const classes[] = {
        "error", "connected", "online", "offline", "pairing", NULL,
    };
    GtkWidget *dot =
        g_object_get_data(G_OBJECT(chip), "bluetooth-chip-dot");
    GtkWidget *label =
        g_object_get_data(G_OBJECT(chip), "bluetooth-chip-label");
    if (label) gtk_label_set_text(GTK_LABEL(label), text ? text : "");
    if (!dot) return;
    for (guint index = 0; classes[index]; index++)
        gtk_widget_remove_css_class(dot, classes[index]);
    if (state) gtk_widget_add_css_class(dot, state);
}

void anto_bluetooth_switch_update(GtkWidget *control, gboolean active,
                                    gboolean sensitive,
                                    const char *tooltip) {
    g_object_set_data(G_OBJECT(control), "bluetooth-syncing",
                      GINT_TO_POINTER(1));
    gtk_switch_set_active(GTK_SWITCH(control), active);
    g_object_set_data(G_OBJECT(control), "bluetooth-syncing", NULL);
    gboolean pending = g_object_get_data(
        G_OBJECT(control), "bluetooth-operation-pending") != NULL;
    gtk_widget_set_sensitive(control, sensitive && !pending);
    if (tooltip) gtk_widget_set_tooltip_text(control, tooltip);
}

void anto_bluetooth_action_button_update(GtkWidget *button,
                                           const char *label,
                                           const char *icon,
                                           const char *argument,
                                           const char *state_class) {
    BluetoothAction *action =
        g_object_get_data(G_OBJECT(button), "bluetooth-action");
    GtkWidget *text =
        g_object_get_data(G_OBJECT(button), "bluetooth-action-label");
    GtkWidget *image =
        g_object_get_data(G_OBJECT(button), "bluetooth-action-icon");
    if (text) gtk_label_set_text(GTK_LABEL(text), label ? label : "");
    if (image && icon) gtk_image_set_from_icon_name(GTK_IMAGE(image), icon);
    if (action) {
        g_free(action->argument);
        action->argument = g_strdup(argument);
    }
    gtk_widget_remove_css_class(button, "primary");
    gtk_widget_remove_css_class(button, "danger");
    if (state_class) gtk_widget_add_css_class(button, state_class);
}

const char *anto_bluetooth_device_display_name(const BluetoothDevice *device) {
    if (anto_bluetooth_present_text(device->alias)) return device->alias;
    if (anto_bluetooth_present_text(device->name)) return device->name;
    return device->address;
}

const char *anto_bluetooth_device_type(const BluetoothDevice *device) {
    const char *icon = device->icon ? device->icon : "";
    if (strstr(icon, "headset") || strstr(icon, "headphones") ||
        strstr(icon, "audio")) return "Audio";
    if (strstr(icon, "keyboard")) return "Tastiera";
    if (strstr(icon, "mouse")) return "Mouse";
    if (strstr(icon, "gaming") || strstr(icon, "joystick")) return "Controller";
    if (strstr(icon, "phone")) return "Telefono";
    if (strstr(icon, "tablet")) return "Tablet";
    if (strstr(icon, "camera")) return "Fotocamera";
    if (strstr(icon, "computer") || strstr(icon, "laptop")) return "Computer";
    if (strstr(icon, "printer")) return "Stampante";
    return "Dispositivo";
}

char *anto_bluetooth_device_metadata(const BluetoothDevice *device) {
    GString *meta = g_string_new(anto_bluetooth_device_type(device));
    if (device->connected) g_string_append(meta, " · connesso");
    else if (device->blocked) g_string_append(meta, " · bloccato");
    else if (device->paired) g_string_append(meta, " · associato");
    else g_string_append(meta, " · rilevato");
    if (device->trusted) g_string_append(meta, " · fidato");
    if (device->rssi != -1) g_string_append_printf(meta, " · RSSI %d dBm", device->rssi);
    return g_string_free(meta, FALSE);
}

void anto_bluetooth_action_bar_append(GtkWidget *bar, GtkWidget *button) {
    gtk_flow_box_append(GTK_FLOW_BOX(bar), button);
}

void anto_bluetooth_action_button_configure(
    GtkWidget *button, const char *label, const char *icon,
    const char *operation, const char *argument, const char *value,
    const char *state_class, gboolean visible, gboolean sensitive) {
    BluetoothAction *action =
        g_object_get_data(G_OBJECT(button), "bluetooth-action");
    GtkWidget *text =
        g_object_get_data(G_OBJECT(button), "bluetooth-action-label");
    GtkWidget *image =
        g_object_get_data(G_OBJECT(button), "bluetooth-action-icon");
    if (text && g_strcmp0(gtk_label_get_text(GTK_LABEL(text)), label) != 0)
        gtk_label_set_text(GTK_LABEL(text), label);
    if (image && icon)
        gtk_image_set_from_icon_name(GTK_IMAGE(image), icon);
    if (action) {
        g_free(action->operation);
        g_free(action->argument);
        g_free(action->value);
        action->operation = g_strdup(operation);
        action->argument = g_strdup(argument);
        action->value = g_strdup(value);
    }
    gtk_widget_remove_css_class(button, "primary");
    gtk_widget_remove_css_class(button, "danger");
    if (state_class) gtk_widget_add_css_class(button, state_class);
    GtkWidget *wrapper = gtk_widget_get_parent(button);
    gtk_widget_set_visible(
        GTK_IS_FLOW_BOX_CHILD(wrapper) ? wrapper : button, visible);
    gtk_widget_set_sensitive(button, sensitive);
}

void anto_bluetooth_device_actions_build_once(MenuApp *app,
                                      BluetoothCardRef *reference,
                                      const BluetoothDevice *device) {
    reference->primary_action = anto_bluetooth_action_button(
        app, "Associa", "list-add-symbolic", "pair", device->address,
        NULL, "primary");
    reference->trust_action = anto_bluetooth_action_button(
        app, "Rendi fidato", "emblem-ok-symbolic", "trust",
        device->address, NULL, NULL);
    reference->block_action = anto_bluetooth_action_button(
        app, "Blocca", "changes-prevent-symbolic", "block",
        device->address, NULL, "danger");
    reference->remove_action = anto_bluetooth_action_button(
        app, "Dimentica", "edit-delete-symbolic", "remove",
        device->address, NULL, "danger");
    for (guint index = 0;
         index < G_N_ELEMENTS(reference->profile_actions); index++)
        reference->profile_actions[index] = anto_bluetooth_action_button(
            app, "Profilo audio", "audio-speakers-symbolic",
            "audio-profile", device->address, NULL, NULL);
    reference->route_action = anto_bluetooth_action_button(
        app, "Usa come uscita", "audio-speakers-symbolic",
        "audio-route", device->address, "output", NULL);

    anto_bluetooth_action_bar_append(reference->actions, reference->primary_action);
    anto_bluetooth_action_bar_append(reference->actions, reference->trust_action);
    anto_bluetooth_action_bar_append(reference->actions, reference->block_action);
    anto_bluetooth_action_bar_append(reference->actions, reference->remove_action);
    for (guint index = 0;
         index < G_N_ELEMENTS(reference->profile_actions); index++)
        anto_bluetooth_action_bar_append(reference->actions,
                          reference->profile_actions[index]);
    anto_bluetooth_action_bar_append(reference->actions, reference->route_action);
}

void anto_bluetooth_device_actions_update(BluetoothCardRef *reference,
                                  const BluetoothDevice *device,
                                  const GPtrArray *profiles) {
    const char *primary_label = device->blocked ? "Sblocca" :
                                device->connected ? "Disconnetti" :
                                device->paired ? "Connetti" : "Associa";
    const char *primary_icon = device->blocked ? "changes-allow-symbolic" :
                               device->connected ? "network-offline-symbolic" :
                               device->paired ? "bluetooth-active-symbolic" :
                                                "list-add-symbolic";
    const char *primary_operation = device->blocked ? "unblock" :
                                    device->connected ? "disconnect" :
                                    device->paired ? "connect" : "pair";
    anto_bluetooth_action_button_configure(
        reference->primary_action, primary_label, primary_icon,
        primary_operation, device->address, NULL,
        device->connected ? NULL : "primary", TRUE, TRUE);
    anto_bluetooth_action_button_configure(
        reference->trust_action,
        device->trusted ? "Non fidato" : "Rendi fidato",
        device->trusted ? "changes-prevent-symbolic" : "emblem-ok-symbolic",
        device->trusted ? "untrust" : "trust", device->address, NULL,
        NULL, device->paired && !device->blocked, TRUE);
    anto_bluetooth_action_button_configure(
        reference->block_action, "Blocca", "changes-prevent-symbolic",
        "block", device->address, NULL, "danger",
        !device->blocked, TRUE);
    anto_bluetooth_action_button_configure(
        reference->remove_action, "Dimentica", "edit-delete-symbolic",
        "remove", device->address, NULL, "danger",
        device->paired, TRUE);

    BluetoothAudioProfile *choices[2] = {0};
    for (guint index = 0; profiles && index < profiles->len; index++) {
        BluetoothAudioProfile *profile =
            g_ptr_array_index((GPtrArray *)profiles, index);
        if (!profile->available ||
            g_strcmp0(profile->profile, "off") == 0)
            continue;
        guint slot = g_str_has_prefix(profile->profile, "a2dp") ? 0 : 1;
        if (!choices[slot] || profile->active) choices[slot] = profile;
    }
    for (guint profile_index = 0; profile_index < G_N_ELEMENTS(choices); profile_index++) {
        BluetoothAudioProfile *profile = choices[profile_index];
        if (!profile) {
            anto_bluetooth_action_button_configure(
                reference->profile_actions[profile_index],
                "Profilo audio", "audio-speakers-symbolic",
                "audio-profile", device->address, NULL, NULL,
                FALSE, FALSE);
            continue;
        }
        anto_bluetooth_action_button_configure(
            reference->profile_actions[profile_index],
            anto_bluetooth_audio_profile_label(profile),
            g_str_has_prefix(profile->profile, "a2dp")
                ? "audio-speakers-symbolic"
                : "audio-input-microphone-symbolic",
            "audio-profile", device->address, profile->profile,
            profile->active ? "primary" : NULL,
            device->connected, !profile->active);
        gtk_widget_set_tooltip_text(reference->profile_actions[profile_index],
            profile->active ? "Profilo audio attivo" : profile->description);
    }

    gboolean has_profiles = profiles && profiles->len > 0;
    anto_bluetooth_action_button_configure(
        reference->route_action, "Usa come uscita",
        "audio-speakers-symbolic", "audio-route", device->address,
        "output", NULL, device->connected && has_profiles, TRUE);
}

void anto_bluetooth_device_status_update(BluetoothCardRef *reference,
                                 const BluetoothDevice *device) {
    gboolean has_battery = device->battery >= 0;
    gtk_widget_set_visible(reference->battery_label, has_battery);
    gtk_widget_set_visible(reference->battery_bar, has_battery);
    gtk_widget_set_visible(reference->state, !has_battery);
    if (has_battery) {
        g_autofree char *battery = g_strdup_printf(
            "%d%%", CLAMP(device->battery, 0, 100));
        gtk_label_set_text(GTK_LABEL(reference->battery_label), battery);
        gtk_level_bar_set_value(GTK_LEVEL_BAR(reference->battery_bar),
                                CLAMP(device->battery, 0, 100));
    } else {
        const char *label = device->blocked ? "BLOCCATO" :
                            device->connected ? "ONLINE" :
                            device->paired ? "SALVATO" : "NUOVO";
        const char *state = device->blocked ? "error" :
                            device->connected ? "connected" :
                            device->paired ? "online" : "pairing";
        anto_bluetooth_state_chip_update(reference->state, label, state);
    }
}

void anto_bluetooth_group_free(gpointer data) {
    g_free(data);
}

void anto_bluetooth_card_ref_free(gpointer data) {
    BluetoothCardRef *reference = data;
    if (!reference) return;
    g_free(reference->address);
    g_free(reference->search_text);
    g_free(reference);
}

void anto_bluetooth_controller_ref_free(gpointer data) {
    BluetoothControllerRef *reference = data;
    if (!reference) return;
    g_free(reference->address);
    g_free(reference);
}

void anto_bluetooth_view_free(gpointer data) {
    BluetoothView *view = data;
    if (!view) return;
    if (view->scroll_restore_id)
        g_source_remove(view->scroll_restore_id);
    g_ptr_array_free(view->groups, TRUE);
    g_ptr_array_free(view->cards, TRUE);
    g_hash_table_unref(view->cards_by_address);
    g_ptr_array_free(view->controllers, TRUE);
    g_hash_table_unref(view->controllers_by_address);
    g_free(view);
}

void anto_bluetooth_status_dot_update(GtkWidget *dot, gboolean active) {
    gtk_widget_remove_css_class(dot, "online");
    gtk_widget_remove_css_class(dot, "offline");
    gtk_widget_add_css_class(dot, active ? "online" : "offline");
}

void anto_bluetooth_empty_update(GtkWidget *empty, const char *title,
                                   const char *subtitle) {
    GtkWidget *title_label =
        g_object_get_data(G_OBJECT(empty), "bluetooth-empty-title");
    GtkWidget *subtitle_label =
        g_object_get_data(G_OBJECT(empty), "bluetooth-empty-subtitle");
    if (title_label)
        gtk_label_set_text(GTK_LABEL(title_label), title ? title : "");
    if (subtitle_label)
        gtk_label_set_text(GTK_LABEL(subtitle_label), subtitle ? subtitle : "");
}

const char *anto_bluetooth_controller_display_name(
    const BluetoothController *controller) {
    if (anto_bluetooth_present_text(controller->alias)) return controller->alias;
    if (anto_bluetooth_present_text(controller->name)) return controller->name;
    return controller->address;
}

void anto_bluetooth_controller_ref_update(
    BluetoothControllerRef *reference,
    const BluetoothController *controller) {
    gtk_image_set_from_icon_name(
        GTK_IMAGE(reference->icon),
        controller->powered ? "bluetooth-active-symbolic" :
                              "bluetooth-disabled-symbolic");
    gtk_label_set_text(GTK_LABEL(reference->name_label),
                       anto_bluetooth_controller_display_name(controller));
    g_autofree char *meta = g_strdup_printf(
        "%s%s%s · radio %s%s%s",
        controller->address ? controller->address : "indirizzo non disponibile",
        anto_bluetooth_present_text(controller->name) ? " · " : "",
        anto_bluetooth_present_text(controller->name) ? controller->name : "",
        controller->powered ? "attiva" : "spenta",
        controller->discovering ? " · scansione" : "",
        controller->discoverable ? " · visibile" : "");
    gtk_label_set_text(GTK_LABEL(reference->meta_label), meta);
    anto_bluetooth_state_chip_update(reference->state,
                      controller->is_default ? "IN USO" : "DISPONIBILE",
                      controller->is_default ? "connected" :
                      controller->powered ? "online" : "offline");
    anto_bluetooth_action_button_update(
        reference->select_button,
        controller->is_default ? "In uso" : "Usa",
        "object-select-symbolic", controller->address,
        controller->is_default ? "primary" : NULL);
    gtk_widget_set_sensitive(reference->select_button,
                             !controller->is_default &&
                             !g_object_get_data(
                                 G_OBJECT(reference->select_button),
                                 "bluetooth-operation-pending"));
}

guint anto_bluetooth_device_group(const BluetoothDevice *device) {
    if (device->connected) return 0;
    if (device->paired) return 1;
    return 2;
}

BluetoothAsyncState *anto_bluetooth_async_state_ref(BluetoothAsyncState *state) {
    g_atomic_int_inc(&state->refs);
    return state;
}

void anto_bluetooth_async_state_unref(gpointer data) {
    BluetoothAsyncState *state = data;
    if (!state || !g_atomic_int_dec_and_test(&state->refs)) return;
    anto_bluetooth_snapshot_free(state->snapshot);
    g_weak_ref_clear(&state->root);
    g_weak_ref_clear(&state->window);
    g_free(state);
}

BluetoothAsyncState *anto_bluetooth_async_state_get(MenuApp *app) {
    static const char state_key[] = "anto-menu-bluetooth-async-state";
    if (!app || !app->window) return NULL;
    BluetoothAsyncState *state =
        g_object_get_data(G_OBJECT(app->window), state_key);
    if (state) return state;

    state = g_new0(BluetoothAsyncState, 1);
    state->refs = 1;
    state->app = app;
    g_weak_ref_init(&state->window, G_OBJECT(app->window));
    g_weak_ref_init(&state->root, NULL);
    g_object_set_data_full(G_OBJECT(app->window), state_key, state,
                           anto_bluetooth_async_state_unref);
    return state;
}

gboolean anto_bluetooth_root_is_current(BluetoothAsyncState *state) {
    GtkWidget *root = g_weak_ref_get(&state->root);
    gboolean current = root && state->app->custom_holder &&
                       gtk_widget_get_parent(root) == state->app->custom_holder;
    g_clear_object(&root);
    return current;
}

void anto_bluetooth_snapshot_finished(GObject *object, GAsyncResult *result,
                                        gpointer data) {
    (void)object;
    BluetoothAsyncState *state = data;
    g_autoptr(GError) error = NULL;
    BluetoothSnapshot *snapshot =
        g_task_propagate_pointer(G_TASK(result), &error);
    state->in_flight = FALSE;

    GObject *window = g_weak_ref_get(&state->window);
    gboolean still_on_page =
        window && g_strcmp0(state->app->current_page, "bluetooth") == 0;
    if (snapshot) {
        anto_bluetooth_snapshot_free(state->snapshot);
        state->snapshot = snapshot;
        snapshot = NULL;
        if (still_on_page) {
            BluetoothView *view = anto_bluetooth_current_view(state);
            if (view)
                anto_bluetooth_view_apply(view, state->snapshot);
        }
    }
    if (error && still_on_page)
        menu_set_footer(state->app, error->message);
    anto_bluetooth_snapshot_free(snapshot);

    gboolean repeat = still_on_page && state->refresh_pending;
    state->refresh_pending = FALSE;
    if (repeat) anto_bluetooth_refresh_start(state);
    g_clear_object(&window);
    anto_bluetooth_async_state_unref(state);
}

void anto_bluetooth_refresh_start(BluetoothAsyncState *state) {
    if (state->in_flight) {
        state->refresh_pending = TRUE;
        return;
    }
    state->in_flight = TRUE;
    GTask *task = g_task_new(NULL, NULL, anto_bluetooth_snapshot_finished,
                             anto_bluetooth_async_state_ref(state));
    g_task_run_in_thread(task, anto_bluetooth_snapshot_load);
    g_object_unref(task);
}

void menu_bluetooth_live_event(MenuApp *app) {
    if (!app || g_strcmp0(app->current_page, "bluetooth") != 0) return;
    BluetoothAsyncState *state = anto_bluetooth_async_state_get(app);
    if (state) anto_bluetooth_refresh_start(state);
}
const char *anto_bluetooth_device_icon(const BluetoothDevice *device) {
    const char *kind = anto_bluetooth_device_type(device);
    if (g_strcmp0(kind, "Audio") == 0) return "audio-headphones-symbolic";
    if (g_strcmp0(kind, "Tastiera") == 0) return "input-keyboard-symbolic";
    if (g_strcmp0(kind, "Mouse") == 0) return "input-mouse-symbolic";
    if (g_strcmp0(kind, "Controller") == 0) return "input-gaming-symbolic";
    if (g_strcmp0(kind, "Telefono") == 0) return "phone-symbolic";
    if (g_strcmp0(kind, "Tablet") == 0) return "input-tablet-symbolic";
    if (g_strcmp0(kind, "Fotocamera") == 0) return "camera-photo-symbolic";
    if (g_strcmp0(kind, "Computer") == 0) return "computer-symbolic";
    if (g_strcmp0(kind, "Stampante") == 0) return "printer-symbolic";
    return device->connected ? "bluetooth-active-symbolic" : "bluetooth-symbolic";
}

const char *anto_bluetooth_audio_profile_label(const BluetoothAudioProfile *profile) {
    if (g_str_has_prefix(profile->profile, "a2dp-sink")) return "Hi-Fi";
    if (g_str_has_prefix(profile->profile, "headset-head-unit")) return "Microfono";
    return anto_bluetooth_present_text(profile->description) ? profile->description : profile->profile;
}
