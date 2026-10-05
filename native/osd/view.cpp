#include "glass.h"
#include "monitor.h"
#include "palette.h"
#include "primitives.h"
#include <algorithm>
#include <gtk/gtk.h>
#include <gtk4-layer-shell.h>
#include <string>

#include "osd.hpp"
void anto_osd_create(Osd *osd) {
  std::string assets =
      std::string(g_get_home_dir()) + "/.local/share/anto-desktop/";
  anto_ui_init(gdk_display_get_default());
  auto *provider = gtk_css_provider_new();
  gtk_css_provider_load_from_path(provider, (assets + "osd.css").c_str());
  gtk_style_context_add_provider_for_display(
      gdk_display_get_default(), GTK_STYLE_PROVIDER(provider),
      GTK_STYLE_PROVIDER_PRIORITY_USER + 3);
  g_object_unref(provider);
  anto_load_glass_style(gdk_display_get_default());
  osd->window = GTK_WINDOW(gtk_application_window_new(osd->application));
  gtk_window_set_title(osd->window, "Anto Desktop OSD");
  gtk_window_set_decorated(osd->window, FALSE);
  gtk_widget_add_css_class(GTK_WIDGET(osd->window), "anto-osd");
  if (anto_layer_shell_supported(gdk_display_get_default())) {
    gtk_layer_init_for_window(osd->window);
    gtk_layer_set_namespace(osd->window, "anto426-osd");
    gtk_layer_set_layer(osd->window, GTK_LAYER_SHELL_LAYER_OVERLAY);
    gtk_layer_set_keyboard_mode(osd->window,
                                GTK_LAYER_SHELL_KEYBOARD_MODE_NONE);
    gtk_layer_set_anchor(osd->window, GTK_LAYER_SHELL_EDGE_BOTTOM, TRUE);
    gtk_layer_set_margin(osd->window, GTK_LAYER_SHELL_EDGE_BOTTOM,
                         ANTO_SIZE_OSD_BOTTOM);
    gtk_layer_set_exclusive_zone(osd->window, -1);
  }
  auto *box = anto_ui_stack(GTK_ORIENTATION_HORIZONTAL, ANTO_SPACING_LG, "osd-panel");
  gtk_window_set_child(osd->window, box);
  anto_glass_bind(osd->window, box, ANTO_RADIUS_PANEL);
  osd->icon = anto_ui_icon("audio-volume-high-symbolic", ANTO_SIZE_ICON_LARGE, "item-icon");
  gtk_image_set_pixel_size(GTK_IMAGE(osd->icon), ANTO_SIZE_ICON_LARGE);
  gtk_box_append(GTK_BOX(box), osd->icon);
  auto *column = anto_ui_stack(GTK_ORIENTATION_VERTICAL, ANTO_SPACING_SM, nullptr);
  gtk_widget_set_size_request(column, 245, -1);
  gtk_box_append(GTK_BOX(box), column);
  auto *heading = anto_ui_stack(GTK_ORIENTATION_HORIZONTAL, ANTO_SPACING_MD, nullptr);
  osd->title = anto_ui_text("", "item-title", 1);
  gtk_widget_set_hexpand(osd->title, TRUE);
  gtk_box_append(GTK_BOX(heading), osd->title);
  osd->value = anto_ui_label("", "osd-value");
  gtk_box_append(GTK_BOX(heading), osd->value);
  gtk_box_append(GTK_BOX(column), heading);
  osd->bar = anto_ui_progress();
  gtk_box_append(GTK_BOX(column), osd->bar);
}
