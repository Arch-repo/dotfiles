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
void Gallery::build_toolbar() {
  auto *toolbar =
      anto_ui_stack(GTK_ORIENTATION_HORIZONTAL, ANTO_SPACING_SM, nullptr);
  if (!embedded_) {
  search_ = anto_ui_search("Cerca nella raccolta…");
  gtk_search_entry_set_placeholder_text(GTK_SEARCH_ENTRY(search_),
                                        "Cerca nella raccolta…");
  gtk_widget_set_hexpand(search_, TRUE);
  g_signal_connect(
      search_, "search-changed",
      reinterpret_cast<GCallback>(+[](GtkSearchEntry *entry, gpointer self) {
        auto *view = static_cast<Gallery *>(self);
        view->query_ = gtk_editable_get_text(GTK_EDITABLE(entry));
        view->filter();
      }),
      this);
  gtk_box_append(GTK_BOX(toolbar), search_);
  }
  GtkToggleButton *first = nullptr;
  const char *filters[] = {"Tutti", "Immagini", "Live"};
  for (int i = 0; i < 3; i++) {
    auto *button =
        GTK_TOGGLE_BUTTON(anto_ui_choice(filters[i]));
    if (first)
      gtk_toggle_button_set_group(button, first);
    else
      first = button;
    g_object_set_data(G_OBJECT(button), "category", GINT_TO_POINTER(i));
    gtk_box_append(GTK_BOX(toolbar), GTK_WIDGET(button));
    g_signal_connect(button, "toggled",
                     reinterpret_cast<GCallback>(
                         +[](GtkToggleButton *widget, gpointer self) {
                           if (!gtk_toggle_button_get_active(widget))
                             return;
                           auto *view = static_cast<Gallery *>(self);
                           view->category_ = GPOINTER_TO_INT(
                               g_object_get_data(G_OBJECT(widget), "category"));
                           view->filter();
                         }),
                     this);
  }
  gtk_toggle_button_set_active(first, TRUE);
  gtk_box_append(GTK_BOX(panel_), toolbar);
}

} // namespace anto
