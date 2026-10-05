#include "glass.h"
#include "monitor.h"
#include "size_bin.h"
#include "widgets.h"
#include <gtk4-layer-shell.h>
static void place(WidgetWindow *w) {
  if (anto_layer_shell_supported(gdk_display_get_default())) {
    gtk_layer_set_margin(w->window, GTK_LAYER_SHELL_EDGE_LEFT, w->geometry.x);
    gtk_layer_set_margin(w->window, GTK_LAYER_SHELL_EDGE_TOP, w->geometry.y);
  }
  GtkWidget *bounds = gtk_window_get_child(w->window);
  anto_size_bin_set_size(bounds, w->geometry.width, w->geometry.height);
  gtk_window_set_default_size(w->window, w->geometry.width, w->geometry.height);
}
static void drag_begin(GtkGestureDrag *gesture, double x, double y,
                       gpointer data) {
  (void)x;
  (void)y;
  WidgetWindow *w = data;
  if (w->app->settings.locked) {
    gtk_gesture_set_state(GTK_GESTURE(gesture), GTK_EVENT_SEQUENCE_DENIED);
    return;
  }
  w->drag_x = w->geometry.x;
  w->drag_y = w->geometry.y;
}
static void drag_update(GtkGestureDrag *gesture, double x, double y,
                        gpointer data) {
  (void)gesture;
  WidgetWindow *w = data;
  if (w->app->settings.locked)
    return;
  GdkRectangle screen;
  gdk_monitor_get_geometry(w->monitor, &screen);
  w->geometry.x =
      CLAMP(w->drag_x + (int)x, 0, MAX(0, screen.width - w->geometry.width));
  w->geometry.y =
      CLAMP(w->drag_y + (int)y, 0, MAX(0, screen.height - w->geometry.height));
  place(w);
}
static void drag_end(GtkGestureDrag *gesture, double x, double y,
                     gpointer data) {
  (void)gesture;
  (void)x;
  (void)y;
  WidgetWindow *w = data;
  if (!w->app->settings.locked) {
    widget_geometry_save(&w->app->settings, w->kind, w->output, w->geometry);
    widget_settings_save(&w->app->settings, NULL);
  }
}
static void resize_begin(GtkGestureDrag *gesture, double x, double y,
                         gpointer data) {
  drag_begin(gesture, x, y, data);
  WidgetWindow *w = data;
  w->drag_x = w->geometry.width;
  w->drag_y = w->geometry.height;
}
static void resize_update(GtkGestureDrag *gesture, double x, double y,
                          gpointer data) {
  (void)gesture;
  WidgetWindow *w = data;
  if (w->app->settings.locked)
    return;
  GdkRectangle screen;
  gdk_monitor_get_geometry(w->monitor, &screen);
  int max_width = MAX(1, screen.width - w->geometry.x),
      max_height = MAX(1, screen.height - w->geometry.y);
  w->geometry.width = CLAMP(w->drag_x + (int)x, MIN(280, max_width), max_width);
  w->geometry.height =
      CLAMP(w->drag_y + (int)y,
            MIN(widget_definitions[w->kind]->height, max_height), max_height);
  place(w);
}
void widget_window_sync(WidgetWindow *w) {
  GdkRectangle screen;
  gdk_monitor_get_geometry(w->monitor, &screen);
  w->geometry = widget_geometry(&w->app->settings, w->kind, w->output,
                                screen.width, screen.height);
  GtkWidget *header = g_object_get_data(G_OBJECT(w->window), "drag-header");
  GtkWidget *resize = g_object_get_data(G_OBJECT(w->window), "resize-handle");
  gtk_widget_set_cursor_from_name(header,
                                  w->app->settings.locked ? "default" : "grab");
  gtk_widget_set_visible(resize, !w->app->settings.locked);
  place(w);
}
static gboolean close_requested(GtkWindow *window, gpointer data) {
  (void)window;
  (void)data;
  return TRUE;
}
static void monitor_geometry_changed(GObject *monitor, GParamSpec *property,
                                     gpointer data) {
  (void)monitor;
  (void)property;
  widget_window_sync(data);
}
WidgetWindow *widget_window_new(WidgetApp *app, WidgetKind kind,
                                GdkMonitor *monitor, const char *output) {
  WidgetWindow *w = g_new0(WidgetWindow, 1);
  w->app = app;
  w->kind = kind;
  w->monitor = g_object_ref(monitor);
  w->output = g_strdup(output);
  GdkRectangle screen;
  gdk_monitor_get_geometry(monitor, &screen);
  w->geometry = widget_geometry(&app->settings, kind, output, screen.width,
                                screen.height);
  w->window = GTK_WINDOW(gtk_application_window_new(app->application));
  g_object_ref(w->window);
  gtk_window_set_title(w->window, widget_definitions[kind]->name);
  gtk_window_set_decorated(w->window, FALSE);
  gtk_widget_add_css_class(GTK_WIDGET(w->window), "anto-widget-window");
  if (anto_layer_shell_supported(gdk_display_get_default())) {
    gtk_layer_init_for_window(w->window);
    gtk_layer_set_namespace(w->window, "anto426-widget");
    gtk_layer_set_layer(w->window, GTK_LAYER_SHELL_LAYER_BOTTOM);
    gtk_layer_set_monitor(w->window, monitor);
    gtk_layer_set_keyboard_mode(w->window, GTK_LAYER_SHELL_KEYBOARD_MODE_NONE);
    gtk_layer_set_exclusive_zone(w->window, -1);
    gtk_layer_set_anchor(w->window, GTK_LAYER_SHELL_EDGE_LEFT, TRUE);
    gtk_layer_set_anchor(w->window, GTK_LAYER_SHELL_EDGE_TOP, TRUE);
  }
  GtkWidget *header = NULL, *panel = widget_view_new(app->store, kind, &header);
  gtk_widget_set_overflow(panel, GTK_OVERFLOW_HIDDEN);
  GtkWidget *overlay = gtk_overlay_new();
  gtk_overlay_set_child(GTK_OVERLAY(overlay), panel);
  GtkWidget *resize = anto_ui_icon("view-fullscreen-symbolic",
                                   ANTO_CONTROL_ACTION_ICON, "widget-resize");
  gtk_widget_set_halign(resize, GTK_ALIGN_END);
  gtk_widget_set_valign(resize, GTK_ALIGN_END);
  gtk_widget_set_margin_end(resize, ANTO_SPACING_SM);
  gtk_widget_set_margin_bottom(resize, ANTO_SPACING_SM);
  gtk_widget_set_cursor_from_name(resize, "se-resize");
  gtk_widget_set_visible(resize, !app->settings.locked);
  gtk_overlay_add_overlay(GTK_OVERLAY(overlay), resize);
  gtk_window_set_child(w->window, anto_size_bin_new(overlay, w->geometry.width,
                                                    w->geometry.height));
  g_object_set_data(G_OBJECT(w->window), "drag-header", header);
  g_object_set_data(G_OBJECT(w->window), "resize-handle", resize);
  GtkGesture *drag = gtk_gesture_drag_new();
  gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(drag), GDK_BUTTON_PRIMARY);
  g_signal_connect(drag, "drag-begin", G_CALLBACK(drag_begin), w);
  g_signal_connect(drag, "drag-update", G_CALLBACK(drag_update), w);
  g_signal_connect(drag, "drag-end", G_CALLBACK(drag_end), w);
  gtk_widget_add_controller(header, GTK_EVENT_CONTROLLER(drag));
  gtk_widget_set_cursor_from_name(header,
                                  app->settings.locked ? "default" : "grab");
  GtkGesture *sizing = gtk_gesture_drag_new();
  g_signal_connect(sizing, "drag-begin", G_CALLBACK(resize_begin), w);
  g_signal_connect(sizing, "drag-update", G_CALLBACK(resize_update), w);
  g_signal_connect(sizing, "drag-end", G_CALLBACK(drag_end), w);
  gtk_widget_add_controller(resize, GTK_EVENT_CONTROLLER(sizing));
  g_signal_connect(w->window, "close-request", G_CALLBACK(close_requested), w);
  anto_glass_bind(w->window, panel, ANTO_RADIUS_PANEL);
  w->geometry_handler = g_signal_connect(
      monitor, "notify::geometry", G_CALLBACK(monitor_geometry_changed), w);
  place(w);
  gtk_widget_set_visible(GTK_WIDGET(w->window), TRUE);
  return w;
}
void widget_window_free(gpointer data) {
  WidgetWindow *w = data;
  g_signal_handler_disconnect(w->monitor, w->geometry_handler);
  gtk_window_destroy(w->window);
  g_object_unref(w->window);
  g_object_unref(w->monitor);
  g_free(w->output);
  g_free(w);
}
