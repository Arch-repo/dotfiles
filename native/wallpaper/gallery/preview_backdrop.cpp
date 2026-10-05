#include "preview_backdrop.hpp"
#include "monitor.h"
#include "primitives.h"
#include "size_bin.h"
#include <gtk4-layer-shell.h>

namespace anto {
PreviewBackdrop::PreviewBackdrop(GtkWindow *owner) {
  window_ = GTK_WINDOW(gtk_window_new());
  g_object_ref(window_);
  gtk_window_set_title(window_, "Anteprima sfondo · Anto Desktop");
  gtk_window_set_decorated(window_, FALSE);
  gtk_window_set_transient_for(window_, owner);
  gtk_window_set_destroy_with_parent(window_, TRUE);
  gtk_widget_add_css_class(GTK_WIDGET(window_), "wallpaper-preview-window");
  if (anto_layer_shell_supported(gdk_display_get_default())) {
    gtk_layer_init_for_window(window_);
    gtk_layer_set_namespace(window_, "anto426-wallpaper-preview");
    gtk_layer_set_layer(window_, GTK_LAYER_SHELL_LAYER_TOP);
    gtk_layer_set_keyboard_mode(window_, GTK_LAYER_SHELL_KEYBOARD_MODE_NONE);
    gtk_layer_set_exclusive_zone(window_, -1);
    for (int edge = 0; edge < GTK_LAYER_SHELL_EDGE_ENTRY_NUMBER; edge++)
      gtk_layer_set_anchor(window_, GtkLayerShellEdge(edge), TRUE);
    auto *monitor = gtk_layer_is_layer_window(owner) ? gtk_layer_get_monitor(owner) : nullptr;
    if (monitor) gtk_layer_set_monitor(window_, monitor);
  } else gtk_window_fullscreen(window_);
  g_signal_connect(window_, "realize", G_CALLBACK(+[](GtkWidget *window, gpointer) {
    auto *surface = gtk_native_get_surface(GTK_NATIVE(window));
    auto *empty = cairo_region_create();
    gdk_surface_set_input_region(surface, empty);
    cairo_region_destroy(empty);
  }), nullptr);
  auto *root = gtk_overlay_new();
  gtk_widget_set_can_target(root, FALSE);
  gtk_window_set_child(window_, root);
  frames_ = gtk_stack_new();
  gtk_stack_set_transition_type(GTK_STACK(frames_), GTK_STACK_TRANSITION_TYPE_CROSSFADE);
  gtk_stack_set_transition_duration(GTK_STACK(frames_), 180);
  gtk_widget_set_opacity(frames_, .82);
  gtk_overlay_set_child(GTK_OVERLAY(root), frames_);
  for (int i = 0; i < 2; i++) {
    pictures_[i] = anto_ui_picture(nullptr);
    auto *bounds = anto_size_bin_new(pictures_[i], 0, 0);
    gtk_widget_set_hexpand(bounds, TRUE);
    gtk_widget_set_vexpand(bounds, TRUE);
    gtk_stack_add_child(GTK_STACK(frames_), bounds);
  }
  auto *dim = anto_ui_stack(GTK_ORIENTATION_VERTICAL, 0, "wallpaper-preview-dim");
  gtk_widget_set_hexpand(dim, TRUE);
  gtk_widget_set_vexpand(dim, TRUE);
  gtk_overlay_add_overlay(GTK_OVERLAY(root), dim);
}
PreviewBackdrop::~PreviewBackdrop() {
  gtk_window_destroy(window_);
  g_object_unref(window_);
}
void PreviewBackdrop::show(GdkPaintable *image) {
  if (!image) { hide(); return; }
  frame_ = 1 - frame_;
  gtk_picture_set_paintable(GTK_PICTURE(pictures_[frame_]), image);
  gtk_stack_set_visible_child(GTK_STACK(frames_), gtk_widget_get_parent(pictures_[frame_]));
  gtk_widget_set_visible(GTK_WIDGET(window_), TRUE);
}
void PreviewBackdrop::hide() {
  gtk_widget_set_visible(GTK_WIDGET(window_), FALSE);
}
}
