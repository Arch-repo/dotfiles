#include "primitives.h"
#include "palette.h"

GtkWidget *anto_ui_icon_button(const char *icon, const char *tooltip) {
    GtkWidget *button = gtk_button_new_from_icon_name(icon);
    gtk_button_set_has_frame(GTK_BUTTON(button), FALSE);
    gtk_widget_add_css_class(button, "round-button");
    if (tooltip) gtk_widget_set_tooltip_text(button, tooltip);
    return button;
}

GtkWidget *anto_ui_action(const char *label, const char *icon, const char *role) {
    GtkWidget *button = gtk_button_new();
    gtk_widget_add_css_class(button, "ui-action");
    if (role) gtk_widget_add_css_class(button, role);
    gboolean navigation = g_strcmp0(role, "ui-navigation") == 0;
    GtkWidget *body = anto_ui_stack(GTK_ORIENTATION_HORIZONTAL, ANTO_SPACING_SM, NULL);
    gtk_widget_set_halign(body, navigation ? GTK_ALIGN_FILL : GTK_ALIGN_CENTER);
    gtk_widget_set_hexpand(body, navigation);
    if (icon && *icon) {
        GtkWidget *image = anto_ui_icon(icon, ANTO_CONTROL_ACTION_ICON, NULL);
        gtk_widget_set_size_request(image, ANTO_CONTROL_ACTION_ICON, ANTO_CONTROL_ACTION_ICON);
        gtk_widget_set_valign(image, GTK_ALIGN_CENTER);
        g_object_set_data(G_OBJECT(button), "ui-action-icon", image);
        gtk_box_append(GTK_BOX(body), image);
    }
    if (label) {
        GtkWidget *text = anto_ui_text(label, "ui-action-label", 1);
        gtk_widget_set_hexpand(text, navigation);
        g_object_set_data(G_OBJECT(button), "ui-action-label", text);
        gtk_box_append(GTK_BOX(body), text);
    }
    gtk_button_set_child(GTK_BUTTON(button), body);
    gtk_widget_set_tooltip_text(button, label);
    return button;
}

GtkWidget *anto_ui_action_label(GtkWidget *button) { return g_object_get_data(G_OBJECT(button), "ui-action-label"); }

GtkWidget *anto_ui_action_icon(GtkWidget *button) { return g_object_get_data(G_OBJECT(button), "ui-action-icon"); }

void anto_ui_action_set_text(GtkWidget *button, const char *text) {
    GtkWidget *label = anto_ui_action_label(button);
    if (label) gtk_label_set_text(GTK_LABEL(label), text ? text : "");
    gtk_widget_set_tooltip_text(button, text);
    gtk_accessible_update_property(GTK_ACCESSIBLE(button), GTK_ACCESSIBLE_PROPERTY_LABEL, text ? text : "", -1);
}

GtkWidget *anto_ui_field(const char *label, const char *placeholder, GtkWidget **entry_out) {
    GtkWidget *field = anto_ui_stack(GTK_ORIENTATION_VERTICAL, ANTO_SPACING_SM, "ui-field");
    gtk_box_append(GTK_BOX(field), anto_ui_text(label, "ui-field-label", 1));
    GtkWidget *entry = gtk_entry_new();
    gtk_widget_add_css_class(entry, "ui-entry");
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry), placeholder);
    gtk_widget_set_hexpand(entry, TRUE);
    gtk_accessible_update_property(GTK_ACCESSIBLE(entry), GTK_ACCESSIBLE_PROPERTY_LABEL, label, -1);
    gtk_box_append(GTK_BOX(field), entry);
    if (entry_out) *entry_out = entry;
    return field;
}

GtkWidget *anto_ui_toggle_row(const char *title, const char *subtitle, gboolean active, GtkWidget **control_out) {
    GtkWidget *row = anto_ui_row(NULL, title, subtitle, NULL, NULL);
    GtkWidget *control = anto_ui_switch(active, title);
    gtk_box_append(GTK_BOX(row), control);
    if (control_out) *control_out = control;
    return row;
}

GtkWidget *anto_ui_search(const char *placeholder) {
    GtkWidget *search = gtk_search_entry_new();
    gtk_widget_add_css_class(search, "ui-search");
    gtk_search_entry_set_placeholder_text(GTK_SEARCH_ENTRY(search), placeholder);
    gtk_widget_set_hexpand(search, TRUE);
    return search;
}

GtkWidget *anto_ui_choice(const char *label) {
    GtkWidget *button = gtk_toggle_button_new_with_label(label);
    gtk_widget_add_css_class(button, "ui-action");
    return button;
}

GtkWidget *anto_ui_password(const char *label) {
    GtkWidget *entry = gtk_password_entry_new();
    gtk_widget_add_css_class(entry, "ui-entry");
    gtk_password_entry_set_show_peek_icon(GTK_PASSWORD_ENTRY(entry), TRUE);
    gtk_widget_set_hexpand(entry, TRUE);
    gtk_accessible_update_property(GTK_ACCESSIBLE(entry), GTK_ACCESSIBLE_PROPERTY_LABEL, label, -1);
    return entry;
}

GtkWidget *anto_ui_switch(gboolean active, const char *label) {
    GtkWidget *control = gtk_switch_new();
    gtk_widget_add_css_class(control, "ui-switch");
    gtk_switch_set_active(GTK_SWITCH(control), active);
    gtk_widget_set_valign(control, GTK_ALIGN_CENTER);
    gtk_accessible_update_property(GTK_ACCESSIBLE(control), GTK_ACCESSIBLE_PROPERTY_LABEL, label ? label : "", -1);
    return control;
}

GtkWidget *anto_ui_scale(double value, double min, double max, double step, const char *label) {
    GtkWidget *scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, min, max, step);
    gtk_widget_add_css_class(scale, "ui-slider");
    gtk_scale_set_draw_value(GTK_SCALE(scale), FALSE);
    gtk_range_set_value(GTK_RANGE(scale), value);
    gtk_accessible_update_property(GTK_ACCESSIBLE(scale), GTK_ACCESSIBLE_PROPERTY_LABEL, label, -1);
    return scale;
}

GtkWidget *anto_ui_progress(void) {
    GtkWidget *bar = gtk_progress_bar_new();
    gtk_widget_add_css_class(bar, "ui-meter");
    return bar;
}

GtkWidget *anto_ui_level(double min, double max) {
    GtkWidget *bar = gtk_level_bar_new_for_interval(min, max);
    gtk_widget_add_css_class(bar, "ui-meter");
    return bar;
}

GtkWidget *anto_ui_text_area(GtkTextBuffer **buffer_out) {
    GtkWidget *text = gtk_text_view_new();
    gtk_widget_add_css_class(text, "ui-entry");
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(text), GTK_WRAP_WORD_CHAR);
    if (buffer_out) *buffer_out = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text));
    return text;
}
GtkWidget *anto_ui_spinner(void) {
    GtkWidget *spinner = gtk_spinner_new();
    gtk_widget_add_css_class(spinner, "ui-spinner");
    gtk_widget_set_size_request(spinner, ANTO_SIZE_ICON_LARGE, ANTO_SIZE_ICON_LARGE);
    gtk_spinner_start(GTK_SPINNER(spinner));
    return spinner;
}
