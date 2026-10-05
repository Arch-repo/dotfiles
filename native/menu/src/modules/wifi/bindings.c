#include "internal.h"
#include "primitives.h"

void anto_wifi_card_update(WifiCardRef *reference,
                             const WifiNetwork *network) {
    if (network->active)
        gtk_widget_add_css_class(reference->card, "connected");
    else
        gtk_widget_remove_css_class(reference->card, "connected");
    if (anto_wifi_present_text(network->security))
        gtk_widget_remove_css_class(reference->card, "open");
    else
        gtk_widget_add_css_class(reference->card, "open");

    gtk_image_set_from_icon_name(GTK_IMAGE(reference->icon),
                                 anto_wifi_signal_icon(network->signal));
    gtk_label_set_text(GTK_LABEL(reference->name), network->ssid);
    gtk_widget_set_visible(reference->saved_chip,
                           network->saved && !network->active);

    const char *security = anto_wifi_present_text(network->security) ?
                           network->security : "Rete aperta";
    const char *band = anto_wifi_network_band(network);
    g_autofree char *detail = network->channel > 0 ?
        g_strdup_printf("%s%s%s · canale %d", security,
                        band ? " · " : "", band ? band : "",
                        network->channel) :
        g_strdup_printf("%s%s%s", security,
                        band ? " · " : "", band ? band : "");
    gtk_label_set_text(GTK_LABEL(reference->meta), detail);

    g_autofree char *percent = g_strdup_printf("%d%%", network->signal);
    gtk_label_set_text(GTK_LABEL(reference->signal_label), percent);
    gtk_level_bar_set_value(GTK_LEVEL_BAR(reference->signal_bar),
                            network->signal);

    const gboolean open = !anto_wifi_present_text(network->security);
    const char *label = network->active ? "Connessa" :
                        network->saved ? "Connetti" :
                        open ? "Connetti" : "Password";
    const char *operation = network->saved ? "connect-saved" :
                            open ? "connect-open" : "password";
    anto_ui_action_set_text(reference->connect_button, label);
    gtk_widget_set_sensitive(
        reference->connect_button,
        !network->active &&
        !g_object_get_data(G_OBJECT(reference->connect_button),
                           "wifi-operation-pending"));
    if (network->active)
        gtk_widget_remove_css_class(reference->connect_button, "primary");
    else
        gtk_widget_add_css_class(reference->connect_button, "primary");
    anto_wifi_action_set(g_object_get_data(
                        G_OBJECT(reference->connect_button),
                        "anto-wifi-action"),
                    operation, network->ssid);

    g_autofree char *haystack = g_strdup_printf(
        "%s %s %s %s %d", network->ssid ? network->ssid : "",
        network->security ? network->security : "",
        network->bssid ? network->bssid : "",
        anto_wifi_network_band(network) ? anto_wifi_network_band(network) : "",
        network->channel);
    g_free(reference->search_text);
    reference->search_text = g_utf8_strdown(haystack, -1);
}

WifiView *anto_wifi_current_view(MenuApp *app) {
    if (!app || app->search_action != anto_wifi_search)
        return NULL;
    return app->search_data;
}

WifiCardRef *anto_wifi_card_find(WifiView *view, const char *key) {
    for (guint index = 0; index < view->cards->len; index++) {
        WifiCardRef *reference = g_ptr_array_index(view->cards, index);
        if (g_strcmp0(reference->key, key) == 0)
            return reference;
    }
    return NULL;
}

void anto_wifi_card_move(WifiView *view, WifiCardRef *reference,
                           guint group_index) {
    if (reference->group_index == group_index) return;
    WifiGroup *old_group =
        g_ptr_array_index(view->groups, reference->group_index);
    WifiGroup *new_group = g_ptr_array_index(view->groups, group_index);
    g_object_ref(reference->card);
    gtk_box_remove(GTK_BOX(old_group->container), reference->card);
    gtk_box_append(GTK_BOX(new_group->container), reference->card);
    g_object_unref(reference->card);
    reference->group_index = group_index;
}

void anto_wifi_cards_reconcile(WifiView *view,
                                 const WifiSnapshot *snapshot) {
    GHashTable *seen =
        g_hash_table_new(g_direct_hash, g_direct_equal);
    GtkWidget *after[WIFI_GROUP_COUNT];
    for (guint index = 0; index < WIFI_GROUP_COUNT; index++) {
        WifiGroup *group = g_ptr_array_index(view->groups, index);
        after[index] = group->header;
    }

    for (guint index = 0; index < snapshot->networks->len; index++) {
        WifiNetwork *network =
            g_ptr_array_index(snapshot->networks, index);
        g_autofree char *key =
            anto_wifi_network_key(network->ssid, network->bssid);
        WifiCardRef *reference = anto_wifi_card_find(view, key);
        guint group_index = anto_wifi_network_group(network);
        if (!reference) {
            reference = anto_wifi_network_card(view->app, network);
            anto_wifi_group_add_card(view, group_index, reference);
        } else {
            anto_wifi_card_move(view, reference, group_index);
            anto_wifi_card_update(reference, network);
        }
        gtk_box_reorder_child_after(
            GTK_BOX(((WifiGroup *)g_ptr_array_index(
                        view->groups, group_index))->container),
            reference->card, after[group_index]);
        after[group_index] = reference->card;
        g_hash_table_add(seen, reference);
    }

    for (guint index = view->cards->len; index > 0; index--) {
        WifiCardRef *reference =
            g_ptr_array_index(view->cards, index - 1);
        if (g_hash_table_contains(seen, reference))
            continue;
        GtkWidget *parent = gtk_widget_get_parent(reference->card);
        if (GTK_IS_BOX(parent))
            gtk_box_remove(GTK_BOX(parent), reference->card);
        g_ptr_array_remove_index(view->cards, index - 1);
    }
    g_hash_table_destroy(seen);
}

void anto_wifi_hero_update(WifiView *view,
                             const WifiSnapshot *snapshot) {
    const gboolean connected =
        g_strcmp0(snapshot->state, "connected") == 0;
    gtk_widget_remove_css_class(view->hero, "loading");
    gtk_widget_remove_css_class(view->hero, "unavailable");
    gtk_widget_remove_css_class(view->hero, "disabled");
    gtk_widget_remove_css_class(view->hero, "connected");
    if (!snapshot->available)
        gtk_widget_add_css_class(view->hero, "unavailable");
    else if (!snapshot->radio_enabled)
        gtk_widget_add_css_class(view->hero, "disabled");
    else if (connected)
        gtk_widget_add_css_class(view->hero, "connected");

    int active_signal = 100;
    for (guint index = 0; index < snapshot->networks->len; index++) {
        WifiNetwork *network =
            g_ptr_array_index(snapshot->networks, index);
        if (network->active) {
            active_signal = network->signal;
            break;
        }
    }
    gtk_image_set_from_icon_name(
        GTK_IMAGE(view->hero_icon),
        snapshot->radio_enabled ? anto_wifi_signal_icon(active_signal) :
                                  "network-wireless-offline-symbolic");

    const char *title = !snapshot->available ?
        "Adattatore non disponibile" :
        !snapshot->radio_enabled ? "Radio in pausa" :
        anto_wifi_present_text(snapshot->ssid) ? snapshot->ssid :
                                       "Pronta a connettere";
    gtk_label_set_text(GTK_LABEL(view->hero_title), title);
    g_autofree char *summary = g_strdup_printf(
        "%s · %s%s%s",
        anto_wifi_connection_state_label(snapshot->state),
        anto_wifi_connectivity_label(snapshot->connectivity),
        anto_wifi_present_text(snapshot->device) ? " · " : "",
        anto_wifi_present_text(snapshot->device) ? snapshot->device : "");
    gtk_label_set_text(GTK_LABEL(view->hero_subtitle), summary);

    gtk_label_set_text(
        GTK_LABEL(view->state_chip),
        !snapshot->available ? "NON DISP." :
        snapshot->radio_enabled ? "RADIO ON" : "RADIO OFF");
    gtk_widget_remove_css_class(view->state_chip, "error");
    gtk_widget_remove_css_class(view->state_chip, "online");
    gtk_widget_remove_css_class(view->state_chip, "offline");
    gtk_widget_add_css_class(
        view->state_chip,
        !snapshot->available ? "error" :
        snapshot->radio_enabled ? "online" : "offline");

    g_signal_handler_block(view->radio_switch, view->radio_handler);
    gtk_switch_set_active(GTK_SWITCH(view->radio_switch),
                          snapshot->radio_enabled);
    g_signal_handler_unblock(view->radio_switch, view->radio_handler);
    gtk_widget_set_sensitive(
        view->radio_switch,
        snapshot->available &&
        !g_object_get_data(G_OBJECT(view->radio_switch),
                           "wifi-operation-pending"));
    gtk_widget_set_sensitive(
        view->refresh_button,
        snapshot->available && snapshot->radio_enabled &&
        !g_object_get_data(G_OBJECT(view->refresh_button),
                           "wifi-operation-pending"));

    gtk_label_set_text(GTK_LABEL(view->ipv4),
                       anto_wifi_present_text(snapshot->ipv4) ?
                       snapshot->ipv4 : "—");
    gtk_label_set_text(GTK_LABEL(view->gateway),
                       anto_wifi_present_text(snapshot->gateway) ?
                       snapshot->gateway : "—");
    gtk_label_set_text(GTK_LABEL(view->dns),
                       anto_wifi_present_text(snapshot->dns) ?
                       snapshot->dns : "—");
    gtk_widget_set_visible(view->metrics, connected);

    gboolean can_disconnect =
        connected && anto_wifi_present_text(snapshot->device);
    WifiAction *disconnect = g_object_get_data(
        G_OBJECT(view->disconnect_button), "anto-wifi-action");
    anto_wifi_action_set(disconnect, "disconnect",
                    can_disconnect ? snapshot->device : NULL);
    gtk_widget_set_visible(view->disconnect_button, can_disconnect);
    gtk_widget_set_sensitive(
        view->disconnect_button,
        can_disconnect &&
        !g_object_get_data(G_OBJECT(view->disconnect_button),
                           "wifi-operation-pending"));

    const char *subtitle = !snapshot->available ?
        "NetworkManager non espone un adattatore wireless" :
        !snapshot->radio_enabled ?
        "Radio spenta · profili e connessioni salvate restano intatti" :
        connected ?
        "Stato, indirizzi e reti aggiornati automaticamente" :
        "Radio attiva · scegli una rete senza interrompere NetworkManager";
    gtk_label_set_text(GTK_LABEL(view->app->page_subtitle), subtitle);
}

void anto_wifi_search(MenuApp *app, const char *query, gpointer data) {
    (void)app;
    WifiView *view = data;
    g_autofree char *needle = g_utf8_strdown(query ? query : "", -1);
    guint total_visible = 0;
    for (guint index = 0; index < view->groups->len; index++) {
        WifiGroup *group = g_ptr_array_index(view->groups, index);
        group->visible_cards = 0;
    }
    for (guint index = 0; index < view->cards->len; index++) {
        WifiCardRef *reference = g_ptr_array_index(view->cards, index);
        gboolean visible = !*needle ||
                           strstr(reference->search_text, needle) != NULL;
        gtk_widget_set_visible(reference->card, visible);
        if (visible) {
            WifiGroup *group =
                g_ptr_array_index(view->groups, reference->group_index);
            group->visible_cards++;
            total_visible++;
        }
    }
    for (guint index = 0; index < view->groups->len; index++) {
        WifiGroup *group = g_ptr_array_index(view->groups, index);
        gtk_widget_set_visible(group->container,
                               view->networks_enabled &&
                               group->visible_cards > 0);
    }
    gtk_widget_set_visible(view->search_empty,
                           view->networks_enabled && *needle &&
                           total_visible == 0);
}

gboolean anto_wifi_restore_scroll(gpointer data) {
    WifiView *view = data;
    view->scroll_restore_id = 0;
    if (g_strcmp0(view->app->current_page, "wifi") != 0 ||
        anto_wifi_current_view(view->app) != view)
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

void anto_wifi_schedule_scroll_restore(WifiView *view, double value) {
    view->scroll_value = value;
    if (!view->scroll_restore_id)
        view->scroll_restore_id = g_idle_add_full(
            G_PRIORITY_DEFAULT_IDLE, anto_wifi_restore_scroll, view, NULL);
}
