#include "../gallery.hpp"
#include "glass.h"
#include "monitor.h"
#include "palette.h"
#include "primitives.h"
#include "size_bin.h"
#include <algorithm>
#include <filesystem>
#include <gtk4-layer-shell.h>
#include <json-c/json.h>
#include <memory>
#include <stdexcept>
namespace fs = std::filesystem;

namespace anto {
static void css_file(const fs::path &file, unsigned priority) {
  auto *provider = gtk_css_provider_new();
  gtk_css_provider_load_from_path(provider, file.c_str());
  gtk_style_context_add_provider_for_display(
      gdk_display_get_default(), GTK_STYLE_PROVIDER(provider), priority);
  g_object_unref(provider);
}
void Gallery::build_window() {
  auto assets = fs::path(g_get_home_dir()) / ".local/share/anto-desktop";
  anto_ui_init(gdk_display_get_default());
  css_file(assets / "wallpaper.css", GTK_STYLE_PROVIDER_PRIORITY_USER + 4);
  anto_load_glass_style(gdk_display_get_default());
  window_ = GTK_WINDOW(gtk_application_window_new(application_));
  gtk_window_set_title(window_, "Sfondi · Anto Desktop");
  gtk_window_set_decorated(window_, FALSE);
  gtk_widget_add_css_class(GTK_WIDGET(window_), "wallpaper-window");
  auto *monitor = anto_active_monitor(gdk_display_get_default());
  GdkRectangle geometry{0, 0, 1600, 1000};
  if (monitor)
    gdk_monitor_get_geometry(monitor, &geometry);
  if (anto_layer_shell_supported(gdk_display_get_default())) {
    gtk_layer_init_for_window(window_);
    gtk_layer_set_namespace(window_, "anto426-wallpaper");
    gtk_layer_set_layer(window_, GTK_LAYER_SHELL_LAYER_OVERLAY);
    gtk_layer_set_keyboard_mode(window_,
                                GTK_LAYER_SHELL_KEYBOARD_MODE_EXCLUSIVE);
    gtk_layer_set_exclusive_zone(window_, -1);
    for (int i = 0; i < GTK_LAYER_SHELL_EDGE_ENTRY_NUMBER; i++)
      gtk_layer_set_anchor(window_, GtkLayerShellEdge(i), TRUE);
    if (monitor)
      gtk_layer_set_monitor(window_, monitor);
  } else
    gtk_window_fullscreen(window_);
  if (monitor)
    g_object_unref(monitor);
  auto *root = gtk_overlay_new();
  gtk_window_set_child(window_, root);
  auto *dim = anto_ui_stack(GTK_ORIENTATION_VERTICAL, 0, "wallpaper-backdrop");
  gtk_widget_set_hexpand(dim, TRUE);
  gtk_widget_set_vexpand(dim, TRUE);
  gtk_overlay_set_child(GTK_OVERLAY(root), dim);
  auto *outside = gtk_gesture_click_new();
  g_signal_connect(
      outside, "pressed",
      reinterpret_cast<GCallback>(
          +[](GtkGestureClick *, int, double, double, gpointer self) {
            gtk_window_close(static_cast<Gallery *>(self)->window_);
          }),
      this);
  gtk_widget_add_controller(dim, GTK_EVENT_CONTROLLER(outside));
  panel_ = anto_ui_stack(GTK_ORIENTATION_VERTICAL, ANTO_SPACING_LG, "wallpaper-panel");
  gtk_widget_set_overflow(panel_, GTK_OVERFLOW_HIDDEN);
  gtk_widget_set_halign(panel_, GTK_ALIGN_FILL);
  gtk_widget_set_valign(panel_, GTK_ALIGN_FILL);
  int panelWidth = std::max(240, std::min(ANTO_SIZE_GALLERY_WIDTH, geometry.width - ANTO_SIZE_MONITOR_MARGIN_X));
  auto *boundedPanel =
      anto_size_bin_new(panel_,
                        panelWidth,
                        std::min(ANTO_SIZE_GALLERY_HEIGHT,
                                 geometry.height - ANTO_SIZE_MONITOR_MARGIN_Y));
  gtk_widget_set_halign(boundedPanel, GTK_ALIGN_CENTER);
  gtk_widget_set_valign(boundedPanel, GTK_ALIGN_CENTER);
  gtk_overlay_add_overlay(GTK_OVERLAY(root), boundedPanel);
  build_header();
  build_toolbar();
  build_content(panelWidth);
  build_footer();
  install_actions();
  anto_glass_bind(window_, panel_, ANTO_RADIUS_PANEL);
}

} // namespace anto
