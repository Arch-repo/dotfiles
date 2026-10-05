#include "menu.h"
#include "ui_internal.h"
#include "local_config.h"
#include "layout.h"
#include <gtk4-layer-shell.h>
#include <json-c/json.h>
#include <stdlib.h>
#include <string.h>
#include "monitor.h"
#include "size_bin.h"
#include "glass.h"
#include "primitives.h"

void menu_target_active_monitor(MenuApp *app) {
    if (!app || !app->window || !gtk_layer_is_layer_window(app->window)) return;

    GdkMonitor *monitor = anto_active_monitor(gtk_widget_get_display(GTK_WIDGET(app->window)));
    if (!monitor) return;
    gtk_layer_set_monitor(app->window, monitor);
    GdkRectangle geometry;
    gdk_monitor_get_geometry(monitor, &geometry);
    AntoLayout layout = anto_menu_layout(geometry.width, geometry.height);
    gtk_widget_set_size_request(app->deck, layout.width, layout.height);
    anto_size_bin_set_size(g_object_get_data(G_OBJECT(app->deck), "panel-bounds"), layout.width, layout.height);
    gtk_widget_set_size_request(app->panel, layout.width - 2 * ANTO_SIZE_PANEL_INSET, layout.height - 2 * ANTO_SIZE_PANEL_INSET);
    gtk_widget_set_visible(app->context_rail, layout.context_visible);
    gboolean compact = layout.width < 680;
    anto_size_bin_set_size(g_object_get_data(G_OBJECT(app->deck), "rail-bounds"),
                          compact ? ANTO_SIZE_SIDEBAR_COMPACT : ANTO_SIZE_SIDEBAR_WIDTH, 0);
    if (compact) gtk_widget_add_css_class(app->nav_rail, "compact");
    else gtk_widget_remove_css_class(app->nav_rail, "compact");
    anto_glass_bind(app->window, app->panel, ANTO_RADIUS_PANEL);
    g_object_unref(monitor);
}
