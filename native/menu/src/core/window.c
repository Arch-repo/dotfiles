#include "shell.h"
#include "glass.h"
#include "size_bin.h"
#include "monitor.h"
#include <gtk4-layer-shell.h>

static void outside_pressed(GtkGestureClick *gesture, int count, double x, double y, gpointer data) {
    (void)gesture; (void)count; (void)x; (void)y;
    menu_close(data);
}

void menu_build_window(MenuApp *app) {
    load_css();
    app->rail_buttons = g_ptr_array_new();
    app->window = GTK_WINDOW(gtk_application_window_new(app->application));
    gtk_window_set_title(app->window, "Anto Desktop");
    gtk_window_set_decorated(app->window, FALSE);
    gtk_window_set_resizable(app->window, TRUE);
    gtk_widget_add_css_class(GTK_WIDGET(app->window), "anto-menu-window");
    if (anto_layer_shell_supported(gdk_display_get_default())) {
        gtk_layer_init_for_window(app->window);
        gtk_layer_set_namespace(app->window, "anto426-menu");
        gtk_layer_set_layer(app->window, GTK_LAYER_SHELL_LAYER_OVERLAY);
        gtk_layer_set_keyboard_mode(app->window, GTK_LAYER_SHELL_KEYBOARD_MODE_EXCLUSIVE);
        gtk_layer_set_exclusive_zone(app->window, -1);
        for (int edge = 0; edge < GTK_LAYER_SHELL_EDGE_ENTRY_NUMBER; edge++)
            gtk_layer_set_anchor(app->window, edge, TRUE);
    } else gtk_window_fullscreen(app->window);

    app->overlay = gtk_overlay_new();
    gtk_window_set_child(app->window, app->overlay);
    GtkWidget *backdrop = anto_ui_stack(GTK_ORIENTATION_VERTICAL, 0, "menu-backdrop");
    gtk_widget_set_hexpand(backdrop, TRUE);
    gtk_widget_set_vexpand(backdrop, TRUE);
    gtk_overlay_set_child(GTK_OVERLAY(app->overlay), backdrop);
    GtkGesture *outside = gtk_gesture_click_new();
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(outside), 0);
    g_signal_connect(outside, "pressed", G_CALLBACK(outside_pressed), app);
    gtk_widget_add_controller(backdrop, GTK_EVENT_CONTROLLER(outside));

    app->deck = gtk_overlay_new();
    gtk_widget_set_size_request(app->deck, ANTO_SIZE_MENU_WIDTH, ANTO_SIZE_MENU_HEIGHT);
    gtk_widget_set_halign(app->deck, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(app->deck, GTK_ALIGN_CENTER);
    gtk_widget_set_overflow(app->deck, GTK_OVERFLOW_HIDDEN);
    gtk_overlay_set_child(GTK_OVERLAY(app->deck), anto_ui_stack(GTK_ORIENTATION_VERTICAL, 0, NULL));
    gtk_overlay_add_overlay(GTK_OVERLAY(app->overlay), app->deck);
    app->panel = anto_ui_stack(GTK_ORIENTATION_HORIZONTAL, 0, "anto-menu-panel");
    gtk_widget_set_overflow(app->panel, GTK_OVERFLOW_HIDDEN);
    gtk_widget_set_size_request(app->panel, ANTO_SIZE_MENU_WIDTH - 2 * ANTO_SIZE_PANEL_INSET,
                               ANTO_SIZE_MENU_HEIGHT - 2 * ANTO_SIZE_PANEL_INSET);
    GtkWidget *bounds = anto_size_bin_new(app->panel, ANTO_SIZE_MENU_WIDTH, ANTO_SIZE_MENU_HEIGHT);
    g_object_set_data(G_OBJECT(app->deck), "panel-bounds", bounds);
    gtk_overlay_add_overlay(GTK_OVERLAY(app->deck), bounds);
    app->nav_rail = menu_ui_sidebar(app);
    GtkWidget *rail_bounds = anto_size_bin_new(app->nav_rail, ANTO_SIZE_SIDEBAR_WIDTH, 0);
    gtk_widget_set_hexpand(rail_bounds, FALSE);
    g_object_set_data(G_OBJECT(app->deck), "rail-bounds", rail_bounds);
    gtk_box_append(GTK_BOX(app->panel), rail_bounds);
    GtkWidget *main = anto_ui_stack(GTK_ORIENTATION_VERTICAL, ANTO_SPACING_LG, "menu-main");
    gtk_widget_set_hexpand(main, TRUE);
    gtk_widget_set_overflow(main, GTK_OVERFLOW_HIDDEN);
    gtk_box_append(GTK_BOX(app->panel), main);
    gtk_box_append(GTK_BOX(main), menu_ui_header(app));
    GtkWidget *content = anto_size_bin_new(menu_ui_content(app), 0, 0);
    gtk_widget_set_vexpand(content, TRUE);
    gtk_box_append(GTK_BOX(main), content);
    app->context_rail = menu_ui_status(app);
    gtk_box_append(GTK_BOX(main), app->context_rail);
    app->footer = anto_ui_text("", "menu-footer", 1);
    gtk_box_append(GTK_BOX(main), app->footer);
    menu_context_start(app);
    anto_glass_bind(app->window, app->panel, ANTO_RADIUS_PANEL);
    GtkEventController *keys = gtk_event_controller_key_new();
    gtk_event_controller_set_propagation_phase(keys, GTK_PHASE_CAPTURE);
    g_signal_connect(keys, "key-pressed", G_CALLBACK(key_pressed), app);
    gtk_widget_add_controller(GTK_WIDGET(app->window), keys);
}
