#include "../gallery.hpp"
#include "primitives.h"
#include "size_bin.h"

namespace anto {
void Gallery::build_preview(GtkWidget *body, int width) {
  auto *preview = gtk_overlay_new();
  gtk_widget_add_css_class(preview, "wallpaper-preview");
  gtk_widget_add_css_class(preview, "ui-media");
  gtk_widget_set_overflow(preview, GTK_OVERFLOW_HIDDEN);
  kind_ = anto_ui_badge("Immagine");
  gtk_widget_set_halign(kind_, GTK_ALIGN_END);
  gtk_widget_set_valign(kind_, GTK_ALIGN_START);
  gtk_widget_set_margin_top(kind_, ANTO_SPACING_MD);
  gtk_widget_set_margin_end(kind_, ANTO_SPACING_MD);
  hero_ = anto_ui_picture(nullptr);
  gtk_widget_add_css_class(hero_, "wallpaper-hero");
  gtk_widget_set_hexpand(hero_, TRUE);
  gtk_widget_set_vexpand(hero_, TRUE);
  gtk_overlay_set_child(GTK_OVERLAY(preview), anto_size_bin_new(hero_, 0, 0));
  auto *caption = anto_ui_copy("Seleziona uno sfondo",
      "La selezione mostra l’anteprima senza applicarla", &title_, &metadata_);
  gtk_widget_add_css_class(caption, "ui-media-caption");
  gtk_widget_set_valign(caption, GTK_ALIGN_END);
  gtk_overlay_add_overlay(GTK_OVERLAY(preview), caption);
  gtk_overlay_add_overlay(GTK_OVERLAY(preview), kind_);
  bool compact = gtk_orientable_get_orientation(GTK_ORIENTABLE(body)) == GTK_ORIENTATION_VERTICAL;
  auto *bounds = anto_size_bin_new(preview, width, compact ? ANTO_SIZE_GALLERY_PREVIEW_COMPACT_HEIGHT : 0);
  gtk_widget_set_hexpand(bounds, TRUE);
  gtk_widget_set_vexpand(bounds, !compact);
  gtk_box_append(GTK_BOX(body), bounds);
}
} // namespace anto
