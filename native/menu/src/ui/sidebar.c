#include "shell.h"
#include <string.h>

typedef struct { const char *page, *icon, *label, *group; } Destination;
static const Destination destinations[] = {
    {"system", "preferences-system-symbolic", "Panoramica", "Desktop"},
    {"apps", "view-app-grid-symbolic", "Applicazioni", NULL},
    {"wallpaper", "preferences-desktop-wallpaper-symbolic", "Sfondi", NULL},
    {"widgets", "view-grid-symbolic", "Widget", NULL},
    {"wifi", "network-wireless-symbolic", "Wi-Fi", "Connessioni"},
    {"bluetooth", "bluetooth-symbolic", "Bluetooth", NULL},
    {"audio", "audio-speakers-symbolic", "Audio", NULL},
    {"brightness", "display-brightness-symbolic", "Energia", "Sistema"},
    {"display", "video-display-symbolic", "Schermi", NULL},
    {"notifications", "preferences-system-notifications-symbolic", "Notifiche", NULL},
    {"calendar", "office-calendar-symbolic", "Calendario", NULL},
    {"keyboard", "input-keyboard-symbolic", "Tastiera", NULL},
    {"capture", "camera-photo-symbolic", "Cattura", "Strumenti"},
    {"record", "media-record-symbolic", "Registrazione", NULL},
    {"clipboard", "edit-paste-symbolic", "Appunti", NULL},
    {"emoji", "face-smile-symbolic", "Emoji", NULL},
    {"floating", "view-restore-symbolic", "Finestre", NULL},
    {"background", "system-run-symbolic", "Processi", NULL},
    {"hardware", "computer-symbolic", "Hardware", NULL},
    {"shortcuts", "preferences-desktop-keyboard-shortcuts-symbolic", "Scorciatoie", NULL},
    {"settings", "emblem-system-symbolic", "Impostazioni", "Preferenze"},
    {"power", "system-shutdown-symbolic", "Sessione", NULL},
};
static void navigate(GtkButton *button, gpointer data) {
    menu_open(data, g_object_get_data(G_OBJECT(button), "menu-page"));
}
void update_rail_state(MenuApp *app) {
    if (!app->rail_buttons) return;
    const char *page = app->current_page;
    if (g_strcmp0(page, "wifi-password") == 0) page = "wifi";
    if (g_strcmp0(page, "calendar-add") == 0) page = "calendar";
    if (g_strcmp0(page, "power-confirm") == 0) page = "power";
    for (guint i = 0; i < app->rail_buttons->len; i++) {
        GtkWidget *button = g_ptr_array_index(app->rail_buttons, i);
        gboolean active = g_strcmp0(page, g_object_get_data(G_OBJECT(button), "menu-page")) == 0;
        if (active) gtk_widget_add_css_class(button, "active");
        else gtk_widget_remove_css_class(button, "active");
        gtk_accessible_update_state(GTK_ACCESSIBLE(button), GTK_ACCESSIBLE_STATE_SELECTED, active, -1);
    }
}
GtkWidget *menu_ui_sidebar(MenuApp *app) {
    GtkWidget *sidebar = anto_ui_stack(GTK_ORIENTATION_VERTICAL, ANTO_SPACING_LG, "menu-sidebar");
    gtk_widget_set_hexpand(sidebar, FALSE);
    GtkWidget *brand = anto_ui_stack(GTK_ORIENTATION_HORIZONTAL, ANTO_SPACING_SM, "sidebar-brand");
    gtk_box_append(GTK_BOX(brand), anto_ui_icon("computer-symbolic", 22, "brand-icon"));
    gtk_box_append(GTK_BOX(brand), anto_ui_text("Il tuo desktop", "sidebar-label", 1));
    gtk_box_append(GTK_BOX(sidebar), brand);
    GtkWidget *list = anto_ui_stack(GTK_ORIENTATION_VERTICAL, 2, "sidebar-items");
    for (guint i = 0; i < G_N_ELEMENTS(destinations); i++) {
        const Destination *item = &destinations[i];
        if (item->group) gtk_box_append(GTK_BOX(list), anto_ui_text(item->group, "sidebar-group", 1));
        GtkWidget *button = anto_ui_action(item->label, item->icon, "ui-navigation");
        gtk_widget_add_css_class(button, "sidebar-item");
        gtk_widget_set_tooltip_text(button, item->label);
        g_object_set_data(G_OBJECT(button), "menu-page", (gpointer)item->page);
        g_signal_connect(button, "clicked", G_CALLBACK(navigate), app);
        g_ptr_array_add(app->rail_buttons, button);
        gtk_box_append(GTK_BOX(list), button);
    }
    GtkWidget *scroll = anto_ui_scroller(list);
    gtk_widget_add_css_class(scroll, "sidebar-scroll");
    gtk_box_append(GTK_BOX(sidebar), scroll);
    GtkWidget *clock = anto_ui_stack(GTK_ORIENTATION_VERTICAL, ANTO_SPACING_XS, "sidebar-clock");
    app->context_time = anto_ui_text("--:--", "context-time", 1);
    app->context_date = anto_ui_text("", "context-date", 1);
    app->context_page = anto_ui_text("", "context-page", 1);
    gtk_widget_set_visible(app->context_page, FALSE);
    gtk_box_append(GTK_BOX(clock), app->context_time);
    gtk_box_append(GTK_BOX(clock), app->context_date);
    gtk_box_append(GTK_BOX(clock), app->context_page);
    gtk_box_append(GTK_BOX(sidebar), clock);
    return sidebar;
}
