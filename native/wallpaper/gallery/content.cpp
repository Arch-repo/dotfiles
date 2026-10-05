#include "../gallery.hpp"
#include "primitives.h"
#include "size_bin.h"
#include <algorithm>

namespace anto {
void Gallery::build_content(int panel_width) {
  bool compact = !embedded_ && panel_width < 640;
  auto *body = anto_ui_stack(compact ? GTK_ORIENTATION_VERTICAL : GTK_ORIENTATION_HORIZONTAL,
                             ANTO_SPACING_LG, "wallpaper-body");
  gtk_widget_set_vexpand(body, TRUE);
  if (!embedded_) build_preview(body, compact ? panel_width - 56 : 0);
  auto *library = anto_ui_stack(GTK_ORIENTATION_VERTICAL, ANTO_SPACING_SM, "wallpaper-library");
  gtk_widget_set_vexpand(library, TRUE);
  model_ = gtk_string_list_new(nullptr);
  selection_ = gtk_single_selection_new(G_LIST_MODEL(g_object_ref(model_)));
  auto *factory = gtk_signal_list_item_factory_new();
  g_signal_connect(factory, "setup", G_CALLBACK(setup), this);
  g_signal_connect(factory, "bind", G_CALLBACK(bind), this);
  grid_ = gtk_grid_view_new(GTK_SELECTION_MODEL(g_object_ref(selection_)), factory);
  gridColumns_ = embedded_ ? std::clamp(panel_width / (ANTO_SIZE_GALLERY_LIBRARY / 2 + 32), 2, 6) : 2;
  gtk_grid_view_set_min_columns(GTK_GRID_VIEW(grid_), gridColumns_);
  gtk_grid_view_set_max_columns(GTK_GRID_VIEW(grid_), gridColumns_);
  gtk_grid_view_set_single_click_activate(GTK_GRID_VIEW(grid_), FALSE);
  gtk_widget_add_css_class(grid_, "ui-collection");
  gtk_widget_add_css_class(grid_, "wallpaper-grid");
  selectionHandler_ = g_signal_connect(selection_, "notify::selected",
      reinterpret_cast<GCallback>(+[](GObject *, GParamSpec *, gpointer self) {
        static_cast<Gallery *>(self)->selected();
      }), this);
  g_signal_connect(grid_, "activate",
      reinterpret_cast<GCallback>(+[](GtkGridView *, guint, gpointer self) {
        static_cast<Gallery *>(self)->apply();
      }), this);
  auto *collection = gtk_overlay_new();
  gtk_widget_set_vexpand(collection, TRUE);
  auto *scroll = anto_ui_scroller(grid_);
  gtk_widget_set_hexpand(scroll, TRUE);
  gtk_widget_add_css_class(scroll, "wallpaper-gallery");
  if (compact) gtk_widget_set_size_request(scroll, -1, 220);
  gtk_overlay_set_child(GTK_OVERLAY(collection), scroll);
  empty_ = anto_ui_empty("folder-pictures-symbolic", "Caricamento raccolta…",
                          "Le anteprime saranno disponibili tra poco", TRUE, &empty_lead_, &empty_title_, &empty_detail_);
  gtk_widget_set_halign(empty_, GTK_ALIGN_FILL);
  gtk_widget_set_valign(empty_, GTK_ALIGN_CENTER);
  gtk_widget_add_css_class(empty_, "wallpaper-empty");
  gtk_overlay_add_overlay(GTK_OVERLAY(collection), empty_);
  gtk_box_append(GTK_BOX(library), collection);
  auto *library_bounds = anto_size_bin_new(library,
      embedded_ || compact ? 0 : std::min(ANTO_SIZE_GALLERY_LIBRARY, panel_width / 3), compact ? 240 : 0);
  gtk_widget_set_hexpand(library_bounds, embedded_ || compact);
  gtk_box_append(GTK_BOX(body), library_bounds);
  gtk_box_append(GTK_BOX(panel_), compact ? anto_ui_scroller(body) : body);
  status_ = anto_ui_text("", "ui-form-status", 2);
  gtk_widget_set_visible(status_, FALSE);
  gtk_box_append(GTK_BOX(panel_), status_);
}

void Gallery::setup(GtkSignalListItemFactory *, GtkListItem *item, gpointer) {
  auto *card = anto_ui_card(GTK_ORIENTATION_VERTICAL);
  gtk_widget_add_css_class(card, "ui-flat");
  gtk_widget_add_css_class(card, "wallpaper-card");
  auto *media = gtk_overlay_new();
  gtk_widget_set_overflow(media, GTK_OVERFLOW_HIDDEN);
  auto *picture = anto_ui_picture(nullptr);
  gtk_overlay_set_child(GTK_OVERLAY(media), picture);
  auto *badge = anto_ui_badge("LIVE");
  gtk_widget_set_halign(badge, GTK_ALIGN_END);
  gtk_widget_set_valign(badge, GTK_ALIGN_START);
  gtk_overlay_add_overlay(GTK_OVERLAY(media), badge);
  gtk_box_append(GTK_BOX(card), anto_size_bin_new(media, 0, ANTO_SIZE_GALLERY_THUMBNAIL_HEIGHT));
  g_object_set_data(G_OBJECT(card), "picture", picture);
  g_object_set_data(G_OBJECT(card), "badge", badge);
  gtk_list_item_set_child(item, card);
}

void Gallery::bind(GtkSignalListItemFactory *, GtkListItem *item, gpointer data) {
  auto *view = static_cast<Gallery *>(data);
  auto *model = GTK_STRING_OBJECT(gtk_list_item_get_item(item));
  if (!model) return;
  auto index = std::stoul(gtk_string_object_get_string(model));
  if (index >= view->catalog_.wallpapers.size()) return;
  const auto &wallpaper = view->catalog_.wallpapers[index];
  auto *card = gtk_list_item_get_child(item);
  auto *picture = GTK_PICTURE(g_object_get_data(G_OBJECT(card), "picture"));
  gtk_picture_set_filename(picture, wallpaper.thumbnail.empty() ? nullptr : wallpaper.thumbnail.c_str());
  gtk_widget_set_visible(GTK_WIDGET(g_object_get_data(G_OBJECT(card), "badge")), wallpaper.live);
  gtk_widget_set_tooltip_text(card, wallpaper.title.c_str());
  gtk_accessible_update_property(GTK_ACCESSIBLE(card), GTK_ACCESSIBLE_PROPERTY_LABEL,
                                   wallpaper.title.c_str(), -1);
}
} // namespace anto
