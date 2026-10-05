#include "internal.h"
#include "primitives.h"

void anto_bluetooth_device_card_update(BluetoothCardRef *reference,
                               const BluetoothDevice *device,
                               const GPtrArray *profiles) {
    gtk_widget_remove_css_class(reference->card, "connected");
    gtk_widget_remove_css_class(reference->card, "blocked");
    if (device->connected)
        gtk_widget_add_css_class(reference->card, "connected");
    if (device->blocked)
        gtk_widget_add_css_class(reference->card, "blocked");
    gtk_image_set_from_icon_name(GTK_IMAGE(reference->icon),
                                 anto_bluetooth_device_icon(device));
    gtk_label_set_text(GTK_LABEL(reference->name_label),
                       anto_bluetooth_device_display_name(device));
    g_autofree char *metadata = anto_bluetooth_device_metadata(device);
    gtk_label_set_text(GTK_LABEL(reference->meta_label), metadata);
    gtk_label_set_text(GTK_LABEL(reference->address_label),
                       device->address);
    anto_bluetooth_device_status_update(reference, device);

    gboolean action_pending = g_object_get_data(
        G_OBJECT(reference->actions),
        "bluetooth-operation-pending") != NULL;
    gtk_widget_set_sensitive(reference->actions, !action_pending);
    anto_bluetooth_device_actions_update(reference, device, profiles);

    g_autofree char *combined = g_strdup_printf(
        "%s %s %s %s %s %s %s", anto_bluetooth_device_display_name(device),
        device->name ? device->name : "", device->address ? device->address : "",
        anto_bluetooth_device_type(device), device->connected ? "connesso online" : "",
        device->paired ? "associato salvato" : "nuovo rilevato",
        device->trusted ? "fidato" : "");
    g_free(reference->search_text);
    reference->search_text = g_utf8_strdown(combined, -1);
}

void anto_bluetooth_search(MenuApp *app, const char *query, gpointer data) {
    (void)app;
    BluetoothView *view = data;
    g_autofree char *needle = g_utf8_strdown(query ? query : "", -1);
    guint total_visible = 0;

    for (guint index = 0; index < view->groups->len; index++) {
        BluetoothGroup *group = g_ptr_array_index(view->groups, index);
        group->visible_cards = 0;
    }
    for (guint index = 0; index < view->cards->len; index++) {
        BluetoothCardRef *reference = g_ptr_array_index(view->cards, index);
        gboolean visible = !*needle || strstr(reference->search_text, needle) != NULL;
        gtk_widget_set_visible(reference->row, visible);
        if (visible) {
            BluetoothGroup *group = g_ptr_array_index(view->groups, reference->group_index);
            group->visible_cards++;
            total_visible++;
        }
    }
    for (guint index = 0; index < view->groups->len; index++) {
        BluetoothGroup *group = g_ptr_array_index(view->groups, index);
        gtk_widget_set_visible(group->container,
                               view->devices_enabled &&
                               group->visible_cards > 0);
    }
    gtk_widget_set_visible(view->search_empty,
                           view->devices_enabled && *needle &&
                           total_visible == 0);
}

gboolean anto_bluetooth_restore_scroll(gpointer data) {
    BluetoothView *view = data;
    view->scroll_restore_id = 0;
    if (g_strcmp0(view->app->current_page, "bluetooth") != 0 ||
        view->app->search_data != view)
        return G_SOURCE_REMOVE;
    GtkAdjustment *adjustment =
        gtk_scrolled_window_get_vadjustment(
            GTK_SCROLLED_WINDOW(view->scroll));
    double lower = gtk_adjustment_get_lower(adjustment);
    double maximum =
        MAX(lower, gtk_adjustment_get_upper(adjustment) -
                       gtk_adjustment_get_page_size(adjustment));
    gtk_adjustment_set_value(
        adjustment, CLAMP(view->scroll_value, lower, maximum));
    return G_SOURCE_REMOVE;
}

void anto_bluetooth_schedule_scroll_restore(BluetoothView *view,
                                              double value) {
    view->scroll_value = value;
    if (!view->scroll_restore_id)
        /* Focus scrolling happens during allocation, after idle callbacks.
         * Restore on the next laid-out frame, including the first snapshot. */
        view->scroll_restore_id = g_timeout_add_full(
            G_PRIORITY_DEFAULT, 50, anto_bluetooth_restore_scroll, view, NULL);
}

void anto_bluetooth_controllers_reconcile(
    BluetoothView *view, const BluetoothSnapshot *snapshot) {
    g_autoptr(GHashTable) seen =
        g_hash_table_new(g_direct_hash, g_direct_equal);
    GtkWidget *after = NULL;
    GPtrArray *controllers = snapshot->controllers;

    for (guint index = 0; controllers && index < controllers->len; index++) {
        BluetoothController *controller =
            g_ptr_array_index(controllers, index);
        if (!controller->address) continue;
        BluetoothControllerRef *reference = g_hash_table_lookup(
            view->controllers_by_address, controller->address);
        if (!reference) {
            reference = anto_bluetooth_controller_ref_new(view, controller);
            gtk_box_append(GTK_BOX(view->adapters_box), reference->row);
            g_ptr_array_add(view->controllers, reference);
            g_hash_table_insert(view->controllers_by_address,
                                g_strdup(reference->address), reference);
        }
        anto_bluetooth_controller_ref_update(reference, controller);
        gtk_box_reorder_child_after(GTK_BOX(view->adapters_box),
                                    reference->row, after);
        after = reference->row;
        g_hash_table_add(seen, reference);
    }

    for (guint index = view->controllers->len; index > 0; index--) {
        BluetoothControllerRef *reference =
            g_ptr_array_index(view->controllers, index - 1);
        if (g_hash_table_contains(seen, reference)) continue;
        g_hash_table_remove(view->controllers_by_address,
                            reference->address);
        if (gtk_widget_get_parent(reference->row) == view->adapters_box)
            gtk_box_remove(GTK_BOX(view->adapters_box), reference->row);
        g_ptr_array_remove_index(view->controllers, index - 1);
    }

    gtk_widget_set_visible(
        view->adapters_section,
        snapshot->status.available && view->controllers->len > 1);
}

void anto_bluetooth_device_card_move(BluetoothView *view,
                                       BluetoothCardRef *reference,
                                       guint group_index) {
    if (reference->group_index == group_index) return;
    BluetoothGroup *old_group =
        g_ptr_array_index(view->groups, reference->group_index);
    BluetoothGroup *new_group =
        g_ptr_array_index(view->groups, group_index);
    g_object_ref(reference->row);
    if (gtk_widget_get_parent(reference->row) == old_group->list)
        gtk_list_box_remove(GTK_LIST_BOX(old_group->list), reference->row);
    gtk_list_box_append(GTK_LIST_BOX(new_group->list), reference->row);
    g_object_unref(reference->row);
    reference->group_index = group_index;
}

void anto_bluetooth_devices_reconcile(
    BluetoothView *view, const BluetoothSnapshot *snapshot) {
    g_autoptr(GHashTable) seen =
        g_hash_table_new(g_direct_hash, g_direct_equal);
    GPtrArray *devices = snapshot->devices;

    for (guint index = 0; devices && index < devices->len; index++) {
        BluetoothDevice *device = g_ptr_array_index(devices, index);
        if (!device->address) continue;
        BluetoothCardRef *reference = g_hash_table_lookup(
            view->cards_by_address, device->address);
        const GPtrArray *profiles = snapshot->audio_profiles
            ? g_hash_table_lookup(snapshot->audio_profiles, device->address)
            : NULL;
        guint group_index = anto_bluetooth_device_group(device);
        if (!reference) {
            reference = anto_bluetooth_device_card_new(view->app, device, profiles);
            reference->group_index = group_index;
            BluetoothGroup *group =
                g_ptr_array_index(view->groups, group_index);
            gtk_list_box_append(GTK_LIST_BOX(group->list), reference->row);
            g_ptr_array_add(view->cards, reference);
            g_hash_table_insert(view->cards_by_address,
                                g_strdup(reference->address), reference);
        } else {
            anto_bluetooth_device_card_move(view, reference, group_index);
            anto_bluetooth_device_card_update(reference, device, profiles);
        }
        g_hash_table_add(seen, reference);
    }

    for (guint index = view->cards->len; index > 0; index--) {
        BluetoothCardRef *reference =
            g_ptr_array_index(view->cards, index - 1);
        if (g_hash_table_contains(seen, reference)) continue;
        g_hash_table_remove(view->cards_by_address, reference->address);
        GtkWidget *parent = gtk_widget_get_parent(reference->row);
        if (GTK_IS_LIST_BOX(parent))
            gtk_list_box_remove(GTK_LIST_BOX(parent), reference->row);
        g_ptr_array_remove_index(view->cards, index - 1);
    }
}

void anto_bluetooth_view_apply(BluetoothView *view,
                                 const BluetoothSnapshot *snapshot) {
    const BluetoothStatus *status = &snapshot->status;
    GtkAdjustment *adjustment =
        gtk_scrolled_window_get_vadjustment(
            GTK_SCROLLED_WINDOW(view->scroll));
    double scroll_value = view->initialized ? gtk_adjustment_get_value(adjustment) : 0;

    const char *subtitle = !status->available ?
        "BlueZ non espone un controller · centro dispositivi non disponibile" :
        !status->powered ?
        "Radio spenta · dispositivi e associazioni restano intatti" :
        status->discovering ?
        "Scansione live · risultati aggiornati senza ricostruire il menu" :
        status->connected_count ?
        "Dispositivi, connessioni e audio wireless" :
        "Radio attiva · avvia il radar per trovare dispositivi";
    gtk_label_set_text(GTK_LABEL(view->app->page_subtitle), subtitle);

    gtk_widget_remove_css_class(view->hero, "offline");
    gtk_widget_remove_css_class(view->hero, "disabled");
    gtk_widget_remove_css_class(view->hero, "powered");
    gtk_widget_remove_css_class(view->hero, "connected");
    if (!status->available)
        gtk_widget_add_css_class(view->hero, "offline");
    else if (!status->powered)
        gtk_widget_add_css_class(view->hero, "disabled");
    else if (status->connected_count)
        gtk_widget_add_css_class(view->hero, "connected");
    else
        gtk_widget_add_css_class(view->hero, "powered");

    gtk_image_set_from_icon_name(
        GTK_IMAGE(view->hero_icon),
        status->available && status->powered ?
            "bluetooth-active-symbolic" : "bluetooth-disabled-symbolic");
    g_autofree char *title = NULL;
    if (!status->available)
        title = g_strdup("Controller non disponibile");
    else if (!status->powered)
        title = g_strdup("Radio Bluetooth spenta");
    else if (status->connected_count)
        title = g_strdup_printf(
            "%u %s conness%s", status->connected_count,
            status->connected_count == 1 ? "dispositivo" : "dispositivi",
            status->connected_count == 1 ? "o" : "i");
    else if (status->discovering)
        title = g_strdup("Radar in ascolto");
    else
        title = g_strdup("Pronto a connettere");
    gtk_label_set_text(GTK_LABEL(view->hero_title), title);

    g_autofree char *identity = status->available
        ? g_strdup_printf("%s%s%s",
                          anto_bluetooth_present_text(status->alias) ? status->alias :
                                                       "Controller BlueZ",
                          anto_bluetooth_present_text(status->address) ? " · " : "",
                          anto_bluetooth_present_text(status->address) ? status->address : "")
        : g_strdup("Servizio o adattatore non rilevato");
    gtk_label_set_text(GTK_LABEL(view->hero_identity), identity);
    g_autofree char *summary = status->available
        ? g_strdup_printf("%u associati · %u connessi · %u rilevati",
                          status->paired_count, status->connected_count,
                          status->seen_count)
        : g_strdup("Controlla BlueZ e la presenza dell’adattatore");
    gtk_label_set_text(GTK_LABEL(view->hero_summary), summary);
    anto_bluetooth_state_chip_update(
        view->hero_state,
        !status->available ? "NON DISP." :
        status->powered ? "RADIO ON" : "RADIO OFF",
        !status->available ? "error" :
        status->powered ? "online" : "offline");
    anto_bluetooth_switch_update(view->power_switch, status->powered,
                            status->available, "Radio Bluetooth");

    gtk_widget_set_visible(view->controller_section, status->available);
    gtk_widget_set_visible(view->scan_spinner,
                           status->available && status->discovering);
    gtk_widget_set_visible(view->scan_icon,
                           !status->discovering);
    gtk_label_set_text(
        GTK_LABEL(view->scan_title),
        status->discovering ? "Radar Bluetooth attivo" :
                              "Ricerca dispositivi");
    gtk_label_set_text(
        GTK_LABEL(view->scan_subtitle),
        status->discovering ?
            "Le nuove presenze compaiono qui senza perdere la selezione" :
            "Trova i dispositivi nelle vicinanze");
    anto_bluetooth_action_button_update(
        view->scan_button,
        status->discovering ? "Ferma" : "Scansiona",
        status->discovering ? "process-stop-symbolic" :
                              "view-refresh-symbolic",
        status->discovering ? "stop" : "start",
        status->discovering ? "danger" : "primary");
    gtk_widget_set_sensitive(view->scan_button,
                             status->available && status->powered &&
                             !g_object_get_data(
                                 G_OBJECT(view->scan_button),
                                 "bluetooth-operation-pending"));
    anto_bluetooth_status_dot_update(view->discoverable_dot,
                                status->discoverable);
    anto_bluetooth_switch_update(
        view->discoverable_switch, status->discoverable,
        status->available && status->powered,
        "Consenti agli altri dispositivi di trovare il computer");
    anto_bluetooth_status_dot_update(view->pairable_dot, status->pairable);
    anto_bluetooth_switch_update(
        view->pairable_switch, status->pairable,
        status->available && status->powered,
        "Permetti nuove associazioni");

    anto_bluetooth_controllers_reconcile(view, snapshot);
    anto_bluetooth_devices_reconcile(view, snapshot);
    view->devices_enabled = status->available && status->powered;
    gtk_widget_set_visible(view->unavailable_empty, !status->available);
    gtk_widget_set_visible(view->paused_empty,
                           status->available && !status->powered);
    anto_bluetooth_empty_update(
        view->devices_empty,
        status->discovering ? "Radar in ascolto…" :
                              "Nessun dispositivo rilevato",
        status->discovering
            ? "Tieni il dispositivo vicino e attiva la modalità di associazione."
            : "Avvia Scansiona per cercare accessori e computer nelle vicinanze.");
    gtk_widget_set_visible(
        view->devices_empty,
        view->devices_enabled &&
        (!snapshot->devices || snapshot->devices->len == 0));
    gtk_widget_set_visible(view->app->search, view->devices_enabled);
    if (!view->initialized && view->devices_enabled)
        gtk_widget_grab_focus(view->app->search);
    view->initialized = TRUE;
    const char *query =
        gtk_editable_get_text(GTK_EDITABLE(view->app->search));
    anto_bluetooth_search(view->app, query, view);
    anto_bluetooth_schedule_scroll_restore(view, scroll_value);
    menu_set_footer(
        view->app,
        status->discovering
            ? "Ricerca in corso · Esc chiude"
            : "Dispositivi aggiornati automaticamente · Esc chiude");
}

BluetoothView *anto_bluetooth_current_view(
    BluetoothAsyncState *state) {
    if (!anto_bluetooth_root_is_current(state) ||
        state->app->search_action != anto_bluetooth_search)
        return NULL;
    GtkWidget *root = g_weak_ref_get(&state->root);
    BluetoothView *view = root
        ? g_object_get_data(G_OBJECT(root), "bluetooth-view") : NULL;
    if (view != state->app->search_data) view = NULL;
    g_clear_object(&root);
    return view;
}
