#include "internal.h"
#include "primitives.h"




GtkWidget *anto_wifi_section_label(const char *text) { return anto_ui_section(text); }


GtkWidget *anto_wifi_state_chip(const char *text, const char *state) {
    GtkWidget *chip = anto_ui_badge(text);
    if (state) gtk_widget_add_css_class(chip, state);
    return chip;
}


GtkWidget *anto_wifi_action_button(MenuApp *app, const char *label, const char *icon,
                                    const char *operation, const char *argument, const char *style) {
    GtkWidget *button = anto_ui_action(label, icon, style);
    gtk_widget_add_css_class(button, "wifi-action-chip");
    WifiAction *action = g_new0(WifiAction, 1);
    *action = (WifiAction){.app = app, .operation = g_strdup(operation), .argument = g_strdup(argument)};
    g_object_set_data(G_OBJECT(button), "anto-wifi-action", action);
    g_signal_connect_data(button, "clicked", G_CALLBACK(anto_wifi_action_clicked), action, anto_wifi_action_free, 0);
    return button;
}


GtkWidget *anto_wifi_editor_button(MenuApp *app) {
    GtkWidget *button = anto_ui_action("Profili", "preferences-system-network-symbolic", NULL);
    g_signal_connect(button, "clicked", G_CALLBACK(anto_wifi_open_editor), app);
    return button;
}


GtkWidget *anto_wifi_metric(const char *name, GtkWidget **value_out) {
    return anto_ui_metric(name, value_out);
}


GtkWidget *anto_wifi_hero(MenuApp *app, WifiView *view) {
    view->hero = anto_ui_card(GTK_ORIENTATION_VERTICAL);
    gtk_widget_add_css_class(view->hero, "wifi-hero");
    view->hero_icon = anto_ui_icon("network-wireless-symbolic", ANTO_CONTROL_ROW_ICON, "item-icon");
    GtkWidget *top = anto_ui_row(view->hero_icon, "Wi-Fi", "Ricerca della connessione…", &view->hero_title, &view->hero_subtitle);
    gtk_widget_add_css_class(top, "ui-flat");
    gtk_widget_add_css_class(view->hero_title, "ui-summary-title");
    view->radio_switch = anto_ui_switch(FALSE, "Attiva Wi-Fi");
    gtk_widget_set_valign(view->radio_switch, GTK_ALIGN_CENTER);
    gtk_widget_set_tooltip_text(view->radio_switch, "Attiva Wi-Fi");
    gtk_widget_set_sensitive(view->radio_switch, FALSE);
    view->radio_handler = g_signal_connect(view->radio_switch, "notify::active", G_CALLBACK(anto_wifi_radio_changed), app);
    gtk_box_append(GTK_BOX(top), view->radio_switch);
    gtk_box_append(GTK_BOX(view->hero), top);
    GtkWidget *bar = anto_ui_stack(GTK_ORIENTATION_HORIZONTAL, ANTO_SPACING_SM, "wifi-action-bar");
    view->state_chip = anto_wifi_state_chip("In attesa", "offline");
    gtk_widget_add_css_class(view->state_chip, "ui-status");
    gtk_widget_set_hexpand(view->state_chip, TRUE);
    gtk_widget_set_halign(view->state_chip, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(bar), view->state_chip);
    view->refresh_button = anto_wifi_action_button(app, "Cerca reti", "view-refresh-symbolic", "rescan", NULL, NULL);
    view->disconnect_button = anto_wifi_action_button(app, "Disconnetti", NULL, "disconnect", NULL, "danger");
    gtk_widget_set_visible(view->disconnect_button, FALSE);
    gtk_box_append(GTK_BOX(bar), view->disconnect_button);
    gtk_box_append(GTK_BOX(bar), anto_wifi_editor_button(app));
    gtk_box_append(GTK_BOX(bar), view->refresh_button);
    gtk_box_append(GTK_BOX(view->hero), bar);
    view->metrics = gtk_grid_new();
    gtk_grid_set_column_homogeneous(GTK_GRID(view->metrics), TRUE);
    gtk_grid_set_column_spacing(GTK_GRID(view->metrics), ANTO_SPACING_SM);
    gtk_grid_attach(GTK_GRID(view->metrics), anto_wifi_metric("Indirizzo IP", &view->ipv4), 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(view->metrics), anto_wifi_metric("Router", &view->gateway), 1, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(view->metrics), anto_wifi_metric("DNS", &view->dns), 2, 0, 1, 1);
    gtk_widget_set_visible(view->metrics, FALSE);
    gtk_box_append(GTK_BOX(view->hero), view->metrics);
    return view->hero;
}


void anto_wifi_show_password(MenuApp *app, const char *ssid) {
    if (app->current_page)
        g_ptr_array_add(app->history, g_strdup(app->current_page));
    g_free(app->current_page);
    app->current_page = g_strdup("wifi-password");
    g_autofree char *subtitle =
        g_strdup_printf("Autenticazione locale per %s", ssid);
    menu_page_begin(app, "network-wireless-encrypted-symbolic",
                    "Password Wi‑Fi", subtitle, "");
    gtk_widget_set_visible(app->search, FALSE);

    GtkWidget *shell = anto_ui_stack(GTK_ORIENTATION_VERTICAL, ANTO_SPACING_MD, "wifi-password-shell");
    GtkWidget *hero = anto_ui_card(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_append(GTK_BOX(hero), anto_ui_icon("network-wireless-encrypted-symbolic", 28, "item-icon"));
    gtk_box_append(GTK_BOX(hero), anto_ui_copy(ssid, "Inserisci la password per collegarti alla rete protetta.", NULL, NULL));
    gtk_box_append(GTK_BOX(shell), hero);
    GtkWidget *form = anto_ui_card(GTK_ORIENTATION_VERTICAL);
    gtk_box_append(GTK_BOX(form), anto_ui_text("Password", "ui-field-label", 1));
    GtkWidget *entry = anto_ui_password("Password Wi-Fi");
    gtk_password_entry_set_show_peek_icon(GTK_PASSWORD_ENTRY(entry), TRUE);
    gtk_accessible_update_property(GTK_ACCESSIBLE(entry), GTK_ACCESSIBLE_PROPERTY_LABEL, "Password Wi-Fi", -1);
    gtk_box_append(GTK_BOX(form), entry);
    GtkWidget *status = anto_ui_text("", "ui-form-status", 2);
    gtk_box_append(GTK_BOX(form), status);
    GtkWidget *buttons = anto_ui_stack(GTK_ORIENTATION_HORIZONTAL, ANTO_SPACING_SM, NULL);
    gtk_widget_set_halign(buttons, GTK_ALIGN_END);
    GtkWidget *cancel = anto_ui_action("Annulla", NULL, NULL);
    GtkWidget *submit = anto_ui_action("Connetti", "network-wireless-symbolic", "primary");
    gtk_box_append(GTK_BOX(buttons), cancel);
    gtk_box_append(GTK_BOX(buttons), submit);
    gtk_box_append(GTK_BOX(form), buttons);
    gtk_box_append(GTK_BOX(shell), form);
    menu_set_custom_content(app, shell);
    menu_set_footer(app,
                    "La password non viene salvata dal menu · Esc chiude");

    WifiPassword *prompt = g_new0(WifiPassword, 1);
    prompt->app = app;
    prompt->entry = entry;
    prompt->submit = submit;
    prompt->status = status;
    prompt->ssid = g_strdup(ssid);
    g_signal_connect_data(submit, "clicked",
                          G_CALLBACK(anto_wifi_password_submit), prompt,
                          anto_wifi_password_free, 0);
    g_signal_connect(cancel, "clicked", G_CALLBACK(anto_wifi_password_cancel), app);
    gtk_widget_grab_focus(entry);
}

WifiGroup *anto_wifi_group_append(WifiView *view, GtkWidget *shell,
                                    const char *title) {
    WifiGroup *group = g_new0(WifiGroup, 1);
    GtkWidget *outer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    group->header = anto_wifi_section_label(title);
    gtk_box_append(GTK_BOX(outer), group->header);
    group->container = outer;
    gtk_widget_set_visible(outer, FALSE);
    g_ptr_array_add(view->groups, group);
    gtk_box_append(GTK_BOX(shell), outer);
    return group;
}

WifiCardRef *anto_wifi_network_card(MenuApp *app, const WifiNetwork *network) {
    WifiCardRef *reference = g_new0(WifiCardRef, 1);
    reference->key = anto_wifi_network_key(network->ssid, network->bssid);
    reference->card = anto_ui_card(GTK_ORIENTATION_VERTICAL);
    reference->icon = anto_ui_icon(anto_wifi_signal_icon(network->signal), ANTO_CONTROL_ROW_ICON, "item-icon");
    GtkWidget *header = anto_ui_row(reference->icon, network->ssid, "Rete wireless", &reference->name, &reference->meta);
    gtk_widget_add_css_class(header, "ui-flat");
    gtk_box_append(GTK_BOX(reference->card), header);
    reference->saved_chip = anto_wifi_state_chip("Salvata", "saved");
    gtk_widget_add_css_class(reference->saved_chip, "ui-status");
    gtk_box_append(GTK_BOX(header), reference->saved_chip);
    GtkWidget *actions = anto_ui_stack(GTK_ORIENTATION_HORIZONTAL, ANTO_SPACING_SM, NULL);
    GtkWidget *signal = anto_ui_stack(GTK_ORIENTATION_VERTICAL, ANTO_SPACING_XS, NULL);
    gtk_widget_set_size_request(signal, 48, -1);
    reference->signal_label = anto_ui_text("", "ui-caption", 1);
    reference->signal_bar = anto_ui_level(0, 100);
    gtk_widget_add_css_class(reference->signal_bar, "wifi-signal-bar");
    gtk_box_append(GTK_BOX(signal), reference->signal_label);
    gtk_box_append(GTK_BOX(signal), reference->signal_bar);
    gtk_widget_set_valign(signal, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(actions), signal);
    gtk_widget_set_hexpand(signal, TRUE);
    reference->connect_button = anto_wifi_connect_button(app, network);
    gtk_widget_add_css_class(reference->connect_button, "ui-action");
    gtk_box_append(GTK_BOX(actions), reference->connect_button);
    gtk_box_append(GTK_BOX(reference->card), actions);
    anto_wifi_card_update(reference, network);
    return reference;
}


void anto_wifi_group_add_card(WifiView *view, guint group_index,
                                WifiCardRef *reference) {
    WifiGroup *group = g_ptr_array_index(view->groups, group_index);
    gtk_box_append(GTK_BOX(group->container), reference->card);
    reference->group_index = group_index;
    g_ptr_array_add(view->cards, reference);
    group->visible_cards++;
}

GtkWidget *anto_wifi_empty(const char *icon, const char *title, const char *detail, gboolean spinner) {
    return anto_ui_empty(icon, title, detail, spinner, NULL, NULL, NULL);
}


void anto_wifi_render(MenuApp *app, const WifiSnapshot *snapshot) {
    menu_page_begin(app, "network-wireless-symbolic", "Wi‑Fi",
                    snapshot ?
                    "Stato, indirizzi e reti aggiornati automaticamente" :
                    "Connessione e reti disponibili",
                    "Cerca rete, sicurezza, banda o BSSID…");

    WifiView *view = g_new0(WifiView, 1);
    view->app = app;
    view->groups = g_ptr_array_new_with_free_func(anto_wifi_group_free);
    view->cards = g_ptr_array_new_with_free_func(anto_wifi_card_ref_free);

    GtkWidget *scroll = gtk_scrolled_window_new();
    view->scroll = scroll;
    gtk_widget_set_hexpand(scroll, TRUE);
    gtk_widget_set_vexpand(scroll, TRUE);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
                                   GTK_POLICY_NEVER,
                                   GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_propagate_natural_width(
        GTK_SCROLLED_WINDOW(scroll), FALSE);
    gtk_scrolled_window_set_propagate_natural_height(
        GTK_SCROLLED_WINDOW(scroll), FALSE);
    GtkWidget *shell = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    view->shell = shell;
    gtk_widget_add_css_class(shell, "wifi-shell");
    gtk_widget_set_hexpand(shell, TRUE);
    gtk_widget_set_valign(shell, GTK_ALIGN_START);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), shell);
    gtk_box_append(GTK_BOX(shell), anto_wifi_hero(app, view));

    view->unavailable_empty = anto_wifi_empty(
        "dialog-warning-symbolic", "Adattatore Wi‑Fi non rilevato",
        "Controlla il dispositivo o il servizio NetworkManager.", FALSE);
    view->radio_empty = anto_wifi_empty(
        "network-wireless-offline-symbolic", "Ricerca reti in pausa",
        "Accendi la radio dal controllo in alto; i profili non verranno modificati.",
        FALSE);
    view->networks_empty = anto_wifi_empty(
        "network-wireless-offline-symbolic", "Nessuna rete rilevata",
        "Premi Aggiorna per richiedere una nuova scansione.", FALSE);
    gtk_widget_set_visible(view->unavailable_empty, FALSE);
    gtk_widget_set_visible(view->radio_empty, FALSE);
    gtk_widget_set_visible(view->networks_empty, FALSE);
    gtk_box_append(GTK_BOX(shell), view->unavailable_empty);
    gtk_box_append(GTK_BOX(shell), view->radio_empty);
    gtk_box_append(GTK_BOX(shell), view->networks_empty);

    anto_wifi_group_append(view, shell, "CONNESSA ORA");
    anto_wifi_group_append(view, shell, "RETI SALVATE");
    anto_wifi_group_append(view, shell, "NELLE VICINANZE");

    view->search_empty = anto_wifi_empty(
        "edit-find-symbolic", "Nessuna rete corrisponde",
        "Prova con nome, protezione, banda o indirizzo BSSID.", FALSE);
    gtk_widget_set_visible(view->search_empty, FALSE);
    gtk_box_append(GTK_BOX(shell), view->search_empty);

    menu_set_custom_content(app, scroll);
    menu_set_search_action(app, anto_wifi_search, view, anto_wifi_view_free);
    if (snapshot)
        anto_wifi_view_apply_snapshot(view, snapshot);
    else
        anto_wifi_search(app, "", view);
    menu_set_footer(
        app,
        "Aggiornamento live · password solo in memoria · nessun riavvio della rete");
}

void menu_show_wifi(MenuApp *app) {
    WifiRuntime *runtime = anto_wifi_runtime_get(app);
    if (runtime->snapshot)
        anto_wifi_render(app, runtime->snapshot);
    else
        anto_wifi_render_loading(app);
    anto_wifi_start_snapshot(app);
}
