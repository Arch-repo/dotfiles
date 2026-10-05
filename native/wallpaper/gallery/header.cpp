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
void Gallery::build_header() {
  auto *header =
      anto_ui_stack(GTK_ORIENTATION_HORIZONTAL, ANTO_SPACING_MD, nullptr);
  header_ = header;
  gtk_box_append(GTK_BOX(panel_), header);
  auto *headings = embedded_
      ? (count_ = anto_ui_text("Caricamento raccolta…", "ui-caption", 1))
      : anto_ui_heading("preferences-desktop-wallpaper-symbolic", "Sfondi", "Caricamento raccolta…", nullptr, nullptr, &count_);
  gtk_widget_set_hexpand(headings, TRUE);
  gtk_box_append(GTK_BOX(header), headings);
  auto *folder = anto_ui_action("Raccolta", "folder-symbolic", nullptr);
  gtk_widget_set_tooltip_text(folder, "Scegli la cartella degli sfondi");
  g_signal_connect(folder, "clicked",
                   reinterpret_cast<GCallback>(+[](GtkButton *, gpointer self) {
                     static_cast<Gallery *>(self)->choose_folder();
                   }),
                   this);
  gtk_box_append(GTK_BOX(header), folder);
  auto *settings = anto_ui_icon_button("emblem-system-symbolic", "Impostazioni");
  gtk_widget_set_tooltip_text(settings,
                              "Impostazioni e destinazioni della palette");
  g_signal_connect(
      settings, "clicked",
      reinterpret_cast<GCallback>(+[](GtkButton *, gpointer self) {
        auto *view = static_cast<Gallery *>(self);
        if (view->navigate_) {
          view->navigate_("settings", view->navigateData_);
          return;
        }
        std::string program =
            (fs::path(g_get_home_dir()) / ".local/bin/anto-menu").string();
        const char *argv[] = {program.c_str(), "settings", nullptr};
        auto *process =
            g_subprocess_newv(argv, G_SUBPROCESS_FLAGS_NONE, nullptr);
        if (process) {
          g_object_unref(process);
          gtk_window_close(view->window_);
        }
      }),
      this);
  gtk_box_append(GTK_BOX(header), settings);
  if (embedded_) return;
  auto *close = anto_ui_icon_button("window-close-symbolic", "Chiudi · Esc");
  gtk_widget_set_tooltip_text(close, "Chiudi · Esc");
  g_signal_connect_swapped(close, "clicked", G_CALLBACK(gtk_window_close),
                           window_);
  gtk_box_append(GTK_BOX(header), close);
}

} // namespace anto
