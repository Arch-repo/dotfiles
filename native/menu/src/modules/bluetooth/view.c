#include "internal.h"
#include "primitives.h"

GtkWidget *anto_bluetooth_action_button(MenuApp *app, const char *label, const char *icon,
                                          const char *operation, const char *argument,
                                          const char *value, const char *style) {
    GtkWidget *button = anto_ui_action(label, icon, style);
    gtk_widget_add_css_class(button, "bluetooth-action-chip");
    GtkWidget *image = anto_ui_action_icon(button);
    GtkWidget *text = anto_ui_action_label(button);
    BluetoothAction *action = anto_bluetooth_action_new(app, operation, argument, value);
    g_object_set_data(G_OBJECT(button), "bluetooth-action", action);
    g_object_set_data(G_OBJECT(button), "bluetooth-action-icon", image);
    g_object_set_data(G_OBJECT(button), "bluetooth-action-label", text);
    g_signal_connect_data(button, "clicked", G_CALLBACK(anto_bluetooth_action_clicked), action, anto_bluetooth_action_free, 0);
    return button;
}


GtkWidget *anto_bluetooth_manager_button(MenuApp *app) {
    GtkWidget *button = anto_ui_action("Gestione avanzata", "preferences-system-bluetooth-symbolic", NULL);
    g_signal_connect(button, "clicked", G_CALLBACK(anto_bluetooth_manager_clicked), app);
    return button;
}


GtkWidget *anto_bluetooth_switch(MenuApp *app, const char *operation,
                                   gboolean active, gboolean sensitive,
                                   const char *tooltip) {
    GtkWidget *control = anto_ui_switch(active, tooltip);
    gtk_widget_add_css_class(control, "bluetooth-controller-toggle");
    gtk_switch_set_active(GTK_SWITCH(control), active);
    gtk_widget_set_sensitive(control, sensitive);
    if (tooltip) gtk_widget_set_tooltip_text(control, tooltip);

    BluetoothSwitchAction *action = g_new0(BluetoothSwitchAction, 1);
    action->app = app;
    action->operation = g_strdup(operation);
    g_signal_connect_data(control, "notify::active", G_CALLBACK(anto_bluetooth_switch_changed),
                          action, anto_bluetooth_switch_action_free, 0);
    return control;
}




GtkWidget *anto_bluetooth_section_label(const char *title) { return anto_ui_section(title); }


GtkWidget *anto_bluetooth_state_chip(const char *text, const char *state) {
    GtkWidget *dot = NULL, *label = NULL;
    GtkWidget *chip = anto_ui_indicator(text, &dot, &label);
    if (state) gtk_widget_add_css_class(dot, state);
    g_object_set_data(G_OBJECT(chip), "bluetooth-chip-dot", dot);
    g_object_set_data(G_OBJECT(chip), "bluetooth-chip-label", label);
    return chip;
}


GtkWidget *anto_bluetooth_action_bar_new(void) {
    GtkWidget *bar = gtk_flow_box_new();
    gtk_widget_add_css_class(bar, "bluetooth-action-bar");
    gtk_flow_box_set_selection_mode(GTK_FLOW_BOX(bar), GTK_SELECTION_NONE);
    gtk_flow_box_set_homogeneous(GTK_FLOW_BOX(bar), FALSE);
    gtk_flow_box_set_column_spacing(GTK_FLOW_BOX(bar), 6);
    gtk_flow_box_set_row_spacing(GTK_FLOW_BOX(bar), 5);
    gtk_flow_box_set_min_children_per_line(GTK_FLOW_BOX(bar), 1);
    gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(bar), 6);
    return bar;
}

BluetoothCardRef *anto_bluetooth_device_card_new(MenuApp *app, const BluetoothDevice *device, const GPtrArray *profiles) {
    BluetoothCardRef *ref = g_new0(BluetoothCardRef, 1);
    ref->address = g_strdup(device->address);
    ref->row = gtk_list_box_row_new();
    gtk_list_box_row_set_selectable(GTK_LIST_BOX_ROW(ref->row), FALSE);
    gtk_list_box_row_set_activatable(GTK_LIST_BOX_ROW(ref->row), FALSE);
    ref->card = anto_ui_card(GTK_ORIENTATION_VERTICAL);
    gtk_widget_add_css_class(ref->card, "bluetooth-device-card");
    gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(ref->row), ref->card);
    ref->icon = anto_ui_icon("bluetooth-symbolic", ANTO_CONTROL_ROW_ICON, "item-icon");
    GtkWidget *top = anto_ui_row(ref->icon, "Dispositivo", "", &ref->name_label, &ref->meta_label);
    gtk_widget_add_css_class(top, "ui-flat");
    ref->address_label = anto_ui_text("", "ui-caption", 1);
    gtk_box_append(GTK_BOX(ref->card), ref->address_label);
    ref->status = anto_ui_stack(GTK_ORIENTATION_VERTICAL, ANTO_SPACING_XS, NULL);
    ref->state = anto_bluetooth_state_chip("", "offline");
    ref->battery_label = anto_ui_text("", "ui-caption", 1);
    ref->battery_bar = anto_ui_level(0, 100);
    gtk_widget_add_css_class(ref->battery_bar, "bluetooth-battery");
    gtk_box_append(GTK_BOX(ref->status), ref->state);
    gtk_box_append(GTK_BOX(ref->status), ref->battery_label);
    gtk_box_append(GTK_BOX(ref->status), ref->battery_bar);
    gtk_box_append(GTK_BOX(top), ref->status);
    gtk_box_append(GTK_BOX(ref->card), top);
    ref->actions = anto_bluetooth_action_bar_new();
    gtk_box_append(GTK_BOX(ref->card), ref->actions);
    anto_bluetooth_device_actions_build_once(app, ref, device);
    anto_bluetooth_device_card_update(ref, device, profiles);
    return ref;
}


BluetoothGroup *anto_bluetooth_device_group_append(BluetoothView *view, GtkWidget *shell,
                                           const char *title) {
    BluetoothGroup *group = g_new0(BluetoothGroup, 1);
    group->container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_box_append(GTK_BOX(group->container), anto_bluetooth_section_label(title));
    group->list = gtk_list_box_new();
    gtk_widget_add_css_class(group->list, "bluetooth-device-list");
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(group->list),
                                    GTK_SELECTION_NONE);
    gtk_box_append(GTK_BOX(group->container), group->list);
    gtk_box_append(GTK_BOX(shell), group->container);
    g_ptr_array_add(view->groups, group);
    return group;
}

GtkWidget *anto_bluetooth_empty_state(const char *icon, const char *title, const char *detail, gboolean spinning) {
    GtkWidget *lead = NULL, *heading = NULL, *subtitle = NULL;
    GtkWidget *empty = anto_ui_empty(icon, title, detail, spinning, &lead, &heading, &subtitle);
    g_object_set_data(G_OBJECT(empty), "bluetooth-empty-lead", lead);
    g_object_set_data(G_OBJECT(empty), "bluetooth-empty-title", heading);
    g_object_set_data(G_OBJECT(empty), "bluetooth-empty-subtitle", subtitle);
    return empty;
}


BluetoothControllerRef *anto_bluetooth_controller_ref_new(
    BluetoothView *view, const BluetoothController *controller) {
    BluetoothControllerRef *reference =
        g_new0(BluetoothControllerRef, 1);
    reference->address = g_strdup(controller->address);
    reference->icon = anto_ui_icon("bluetooth-active-symbolic", ANTO_CONTROL_ROW_ICON, "item-icon");
    reference->row = anto_ui_row(reference->icon, "", "", &reference->name_label, &reference->meta_label);
    reference->state = anto_bluetooth_state_chip("", "offline");
    gtk_box_append(GTK_BOX(reference->row), reference->state);
    reference->select_button = anto_bluetooth_action_button(
        view->app, "Usa", "object-select-symbolic", "controller-select",
        controller->address, NULL, NULL);
    gtk_box_append(GTK_BOX(reference->row), reference->select_button);
    return reference;
}

GtkWidget *anto_bluetooth_scroller_new(GtkWidget **shell_out) {
    *shell_out = anto_ui_stack(GTK_ORIENTATION_VERTICAL, ANTO_SPACING_MD, "bluetooth-shell");
    gtk_widget_set_valign(*shell_out, GTK_ALIGN_START);
    return anto_ui_scroller(*shell_out);
}

void anto_bluetooth_build_once(MenuApp *app,
                                 BluetoothAsyncState *state) {
    menu_page_begin(app, "bluetooth-active-symbolic", "Bluetooth",
                    "Dispositivi, connessioni e audio wireless",
                    "Cerca dispositivo, tipo o indirizzo…");
    BluetoothView *view = g_new0(BluetoothView, 1);
    view->app = app;
    view->groups = g_ptr_array_new_with_free_func(anto_bluetooth_group_free);
    view->cards = g_ptr_array_new_with_free_func(anto_bluetooth_card_ref_free);
    view->cards_by_address =
        g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
    view->controllers =
        g_ptr_array_new_with_free_func(anto_bluetooth_controller_ref_free);
    view->controllers_by_address =
        g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
    view->scroll = anto_bluetooth_scroller_new(&view->shell);

    anto_bluetooth_build_hero_live(view);
    anto_bluetooth_build_controller_panel_live(view);
    gtk_widget_set_visible(view->controller_section, FALSE);

    view->adapters_section =
        gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_box_append(GTK_BOX(view->adapters_section),
                   anto_bluetooth_section_label("ADATTATORI BLUETOOTH"));
    view->adapters_box =
        gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_box_append(GTK_BOX(view->adapters_section),
                   view->adapters_box);
    gtk_box_append(GTK_BOX(view->shell), view->adapters_section);
    gtk_widget_set_visible(view->adapters_section, FALSE);

    view->unavailable_empty = anto_bluetooth_empty_state(
        "dialog-warning-symbolic", "Controller Bluetooth non rilevato",
        "Controlla che BlueZ sia attivo e che l’adattatore sia presente.",
        FALSE);
    gtk_box_append(GTK_BOX(view->shell), view->unavailable_empty);
    gtk_widget_set_visible(view->unavailable_empty, FALSE);

    view->paused_empty = anto_bluetooth_empty_state(
        "bluetooth-disabled-symbolic", "Il centro dispositivi è in pausa",
        "Accendi la radio in alto: associazioni e dispositivi restano intatti.",
        FALSE);
    gtk_box_append(GTK_BOX(view->shell), view->paused_empty);
    gtk_widget_set_visible(view->paused_empty, FALSE);

    anto_bluetooth_device_group_append(view, view->shell, "CONNESSI ORA");
    anto_bluetooth_device_group_append(view, view->shell, "SALVATI");
    anto_bluetooth_device_group_append(view, view->shell, "NELLE VICINANZE");
    for (guint index = 0; index < view->groups->len; index++) {
        BluetoothGroup *group = g_ptr_array_index(view->groups, index);
        gtk_widget_set_visible(group->container, FALSE);
    }

    view->devices_empty = anto_bluetooth_empty_state(
        "bluetooth-symbolic", "Sincronizzazione dispositivi…",
        "Lo stato dei dispositivi arriva in background senza bloccare il menu.",
        FALSE);
    gtk_box_append(GTK_BOX(view->shell), view->devices_empty);
    gtk_widget_set_visible(view->devices_empty, FALSE);
    view->search_empty = anto_bluetooth_empty_state(
        "edit-find-symbolic", "Nessun dispositivo corrisponde",
        "Prova con nome, categoria o indirizzo Bluetooth.", FALSE);
    gtk_box_append(GTK_BOX(view->shell), view->search_empty);
    gtk_widget_set_visible(view->search_empty, FALSE);

    gtk_widget_set_visible(app->search, FALSE);
    menu_set_custom_content(app, view->scroll);
    g_weak_ref_set(&state->root, G_OBJECT(view->scroll));
    g_object_set_data(G_OBJECT(view->scroll), "bluetooth-view", view);
    menu_set_search_action(app, anto_bluetooth_search, view, anto_bluetooth_view_free);
    if (state->snapshot)
        anto_bluetooth_view_apply(view, state->snapshot);
    else
        menu_set_footer(
            app,
            "Aggiornamento live in corso · il menu resta reattivo");
}

void menu_show_bluetooth(MenuApp *app) {
    menu_bluetooth_agent_start(app);
    BluetoothAsyncState *state = anto_bluetooth_async_state_get(app);
    if (!state) return;
    if (!anto_bluetooth_root_is_current(state))
        anto_bluetooth_build_once(app, state);
    anto_bluetooth_refresh_start(state);
}
void anto_bluetooth_build_hero_live(BluetoothView *view) {
    view->hero = anto_ui_card(GTK_ORIENTATION_VERTICAL);
    view->hero_icon = anto_ui_icon("bluetooth-active-symbolic", ANTO_CONTROL_ROW_ICON, "item-icon");
    GtkWidget *top = anto_ui_row(view->hero_icon, "Bluetooth", "", &view->hero_title, &view->hero_identity);
    gtk_widget_add_css_class(top, "ui-flat");
    gtk_widget_add_css_class(view->hero_title, "ui-summary-title");
    view->hero_summary = anto_ui_text("", "ui-caption", 1);
    view->power_switch = anto_bluetooth_switch(view->app, "power", FALSE, FALSE, "Attiva Bluetooth");
    gtk_widget_set_valign(view->power_switch, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(top), view->power_switch);
    gtk_box_append(GTK_BOX(view->hero), top);
    gtk_box_append(GTK_BOX(view->hero), view->hero_summary);
    view->hero_state = anto_bluetooth_state_chip("In attesa", "pairing");
    gtk_widget_set_halign(view->hero_state, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(view->hero), view->hero_state);
    gtk_box_append(GTK_BOX(view->shell), view->hero);
}

void anto_bluetooth_build_controller_panel_live(BluetoothView *view) {
    MenuApp *app = view->app;
    view->controller_section = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_box_append(GTK_BOX(view->controller_section),
                   anto_bluetooth_section_label("CONTROLLO RADIO"));
    view->controller_panel = anto_ui_stack(GTK_ORIENTATION_VERTICAL, ANTO_SPACING_XS, NULL);
    GtkWidget *scan = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    view->scan_icon_holder = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    view->scan_spinner = anto_ui_spinner();
    gtk_widget_add_css_class(view->scan_spinner, "bluetooth-device-icon");
    gtk_spinner_start(GTK_SPINNER(view->scan_spinner));
    view->scan_icon =
        anto_ui_icon("find-location-symbolic", ANTO_CONTROL_ROW_ICON, "item-icon");
    gtk_image_set_pixel_size(GTK_IMAGE(view->scan_icon), 20);
    gtk_widget_add_css_class(view->scan_icon, "bluetooth-device-icon");
    gtk_box_append(GTK_BOX(view->scan_icon_holder), view->scan_spinner);
    gtk_box_append(GTK_BOX(view->scan_icon_holder), view->scan_icon);
    gtk_widget_set_visible(view->scan_spinner, FALSE);
    gtk_box_append(GTK_BOX(scan), view->scan_icon_holder);
    GtkWidget *copy = anto_ui_copy("", "", &view->scan_title, &view->scan_subtitle);
    gtk_widget_set_visible(view->scan_subtitle, TRUE);
    gtk_box_append(GTK_BOX(scan), copy);
    gtk_widget_add_css_class(scan, "ui-row");
    view->scan_button = anto_bluetooth_action_button(
        app, "Scansiona", "view-refresh-symbolic", "scan", "start",
        NULL, "primary");
    gtk_box_append(GTK_BOX(scan), view->scan_button);
    gtk_box_append(GTK_BOX(view->controller_panel), scan);

    gtk_box_append(GTK_BOX(view->controller_panel), anto_bluetooth_controller_setting_live(
        app, "Visibile ai dispositivi", "Consenti agli altri dispositivi di trovare il computer",
        "discoverable", &view->discoverable_dot,
        &view->discoverable_switch));
    gtk_box_append(GTK_BOX(view->controller_panel), anto_bluetooth_controller_setting_live(
        app, "Accetta associazioni", "Permetti nuove associazioni",
        "pairable", &view->pairable_dot, &view->pairable_switch));

    GtkWidget *bar = anto_bluetooth_action_bar_new();
    anto_bluetooth_action_bar_append(bar, anto_bluetooth_action_button(
        app, "Aggiorna", "view-refresh-symbolic", "status", NULL, NULL, NULL));
    anto_bluetooth_action_bar_append(bar, anto_bluetooth_manager_button(app));
    gtk_box_append(GTK_BOX(view->controller_panel), bar);
    gtk_box_append(GTK_BOX(view->controller_section),
                   view->controller_panel);
    gtk_box_append(GTK_BOX(view->shell), view->controller_section);
}

GtkWidget *anto_bluetooth_controller_setting_live(MenuApp *app, const char *title, const char *subtitle, const char *operation, GtkWidget **dot_out, GtkWidget **switch_out) {
    GtkWidget *row = anto_ui_row(NULL, title, subtitle, NULL, NULL);
    GtkWidget *dot = anto_ui_stack(GTK_ORIENTATION_HORIZONTAL, 0, "ui-status-dot");
    gtk_widget_set_valign(dot, GTK_ALIGN_CENTER);
    gtk_widget_add_css_class(dot, "offline");
    gtk_box_append(GTK_BOX(row), dot);
    GtkWidget *control = anto_bluetooth_switch(app, operation, FALSE, FALSE, title);
    gtk_box_append(GTK_BOX(row), control);
    *dot_out = dot;
    *switch_out = control;
    return row;
}
