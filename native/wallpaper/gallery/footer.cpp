#include "../gallery.hpp"
#include "primitives.h"

namespace anto {
void Gallery::build_footer() {
  applyAction_ = g_simple_action_new("apply", nullptr);
  g_signal_connect(applyAction_, "activate", reinterpret_cast<GCallback>(
      +[](GSimpleAction *, GVariant *, gpointer self) { static_cast<Gallery *>(self)->apply(); }), this);
  auto *actions = g_simple_action_group_new();
  g_action_map_add_action(G_ACTION_MAP(actions), G_ACTION(applyAction_));
  g_object_unref(applyAction_);
  bootAction_ = g_simple_action_new("boot", nullptr);
  g_signal_connect(bootAction_, "activate", reinterpret_cast<GCallback>(
      +[](GSimpleAction *, GVariant *, gpointer self) { static_cast<Gallery *>(self)->apply("__boot_login__"); }), this);
  g_action_map_add_action(G_ACTION_MAP(actions), G_ACTION(bootAction_));
  g_object_unref(bootAction_);
  gtk_widget_insert_action_group(panel_, "wallpaper", G_ACTION_GROUP(actions));
  g_object_unref(actions);
  set_can_apply(false);
  if (embedded_) return;
  auto *footer = anto_ui_stack(GTK_ORIENTATION_HORIZONTAL, ANTO_SPACING_LG, "wallpaper-footer");
  auto *hint = anto_ui_text("Invio applica · Esc chiude", "ui-caption", 1);
  gtk_widget_set_hexpand(hint, TRUE);
  gtk_box_append(GTK_BOX(footer), hint);
  auto *boot = anto_ui_action("GRUB e login", "system-reboot-symbolic", nullptr);
  gtk_widget_add_css_class(boot, "wallpaper-boot-action");
  gtk_widget_set_tooltip_text(boot, "Applica la selezione ad avvio e login, conservando lo sfondo desktop");
  gtk_actionable_set_action_name(GTK_ACTIONABLE(boot), "wallpaper.boot");
  gtk_box_append(GTK_BOX(footer), boot);
  apply_ = anto_ui_action("Applica sfondo", "object-select-symbolic", "primary");
  gtk_actionable_set_action_name(GTK_ACTIONABLE(apply_), "wallpaper.apply");
  gtk_box_append(GTK_BOX(footer), apply_);
  gtk_box_append(GTK_BOX(panel_), footer);
}
void Gallery::set_can_apply(bool enabled) {
  g_simple_action_set_enabled(applyAction_, enabled);
  g_simple_action_set_enabled(bootAction_, enabled);
}
} // namespace anto
