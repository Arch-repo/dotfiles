#include "shell.h"
static void navigate(GtkButton *button, gpointer data) {
    menu_open(data, g_object_get_data(G_OBJECT(button), "menu-page"));
}
static GtkWidget *status(MenuApp *app, const char *page, const char *title, const char *icon, GtkWidget **icon_out, GtkWidget **value_out) {
    GtkWidget *button = anto_ui_action("…", icon, "ui-status-action");
    gtk_widget_set_hexpand(button, TRUE);
    g_object_set_data(G_OBJECT(button), "menu-page", (gpointer)page);
    g_signal_connect(button, "clicked", G_CALLBACK(navigate), app);
    gtk_widget_set_tooltip_text(button, title);
    *icon_out = anto_ui_action_icon(button);
    *value_out = anto_ui_action_label(button);
    gtk_widget_add_css_class(*icon_out, "context-icon");
    gtk_widget_add_css_class(*value_out, "context-card-value");
    gtk_label_set_max_width_chars(GTK_LABEL(*value_out), 14);
    return button;
}

GtkWidget *menu_ui_status(MenuApp *app) {
    GtkWidget *strip = anto_ui_stack(GTK_ORIENTATION_HORIZONTAL, ANTO_SPACING_XS, "context-strip");
    app->context_network_card = status(app, "wifi", "Connessione", "network-offline-symbolic", &app->context_network_icon, &app->context_network_value);
    app->context_audio_card = status(app, "audio", "Audio", "audio-volume-muted-symbolic", &app->context_audio_icon, &app->context_audio_value);
    app->context_display_card = status(app, "display", "Schermi", "video-display-symbolic", &app->context_display_icon, &app->context_display_value);
    app->context_battery_card = status(app, "hardware", "Batteria", "battery-good-symbolic", &app->context_battery_icon, &app->context_battery_value);
    gtk_box_append(GTK_BOX(strip), app->context_network_card);
    gtk_box_append(GTK_BOX(strip), app->context_audio_card);
    gtk_box_append(GTK_BOX(strip), app->context_display_card);
    gtk_box_append(GTK_BOX(strip), app->context_battery_card);
    return strip;
}
