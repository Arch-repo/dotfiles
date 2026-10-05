#include "../apply.hpp"
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
void Gallery::apply(const char *target) {
  auto *item = selected_wallpaper();
  if (!item)
    return;
  g_autoptr(GError) error = nullptr;
  if (launch_wallpaper_apply(item->path, target, &error))
    gtk_window_close(window_);
  else {
    gtk_label_set_text(GTK_LABEL(status_),
                       error ? error->message : "Operazione non disponibile");
    gtk_widget_set_visible(status_, TRUE);
  }
}

void Gallery::choose_folder() {
  auto *dialog = gtk_file_dialog_new();
  gtk_file_dialog_set_title(dialog, "Cartella degli sfondi");
  gtk_file_dialog_select_folder(
      dialog, window_, cancellation_,
      +[](GObject *source, GAsyncResult *result, gpointer data) {
        std::unique_ptr<std::weak_ptr<Lifetime>> lifetime(static_cast<std::weak_ptr<Lifetime> *>(data));
        auto owner = lifetime->lock();
        GError *error = nullptr;
        auto *folder = gtk_file_dialog_select_folder_finish(
            GTK_FILE_DIALOG(source), result, &error);
        if (folder && owner) {
          auto *view = owner->view;
          gchar *path = g_file_get_path(folder);
          if (path) {
            auto config = fs::path(g_get_user_config_dir()) /
                          "anto426-local/wallpaper/gallery.json";
            std::error_code directoryError;
            fs::create_directories(config.parent_path(), directoryError);
            auto *object = json_object_new_object();
            json_object_object_add(object, "directory",
                                   json_object_new_string(path));
            bool saved = g_file_set_contents_full(
                config.c_str(), json_object_to_json_string(object), -1,
                GFileSetContentsFlags(G_FILE_SET_CONTENTS_CONSISTENT |
                                      G_FILE_SET_CONTENTS_DURABLE),
                0600, &error);
            json_object_put(object);
            g_free(path);
            if (saved)
              view->load();
            else {
              gtk_label_set_text(GTK_LABEL(view->status_),
                                 error ? error->message
                                       : "Salvataggio non riuscito");
              gtk_widget_set_visible(view->status_, TRUE);
            }
          }
        }
        if (folder) g_object_unref(folder);
        g_clear_error(&error);
        g_object_unref(source);
      },
      new std::weak_ptr<Lifetime>(lifetime_));
}

gboolean Gallery::key(GtkEventControllerKey *, guint keyval, guint,
                      GdkModifierType modifiers, gpointer data) {
  auto *view = static_cast<Gallery *>(data);
  return view->handle_key(keyval, modifiers);
}
gboolean Gallery::handle_key(guint keyval, GdkModifierType modifiers) {
  auto *view = this;
  if (keyval == GDK_KEY_Escape) {
    if (embedded_) return FALSE;
    gtk_window_close(view->window_);
    return TRUE;
  }
  if (keyval == GDK_KEY_slash) {
    gtk_widget_grab_focus(view->search_);
    return TRUE;
  }
  if (modifiers == GDK_CONTROL_MASK &&
      (keyval == GDK_KEY_Return || keyval == GDK_KEY_KP_Enter)) {
    gtk_widget_activate_action(panel_, "wallpaper.boot", nullptr);
    return TRUE;
  }
  auto *focus = gtk_window_get_focus(window_);
  bool searching = focus && (focus == search_ || gtk_widget_is_ancestor(focus, search_));
  bool arrow = keyval == GDK_KEY_Left || keyval == GDK_KEY_Right ||
               keyval == GDK_KEY_Up || keyval == GDK_KEY_Down;
  if (!arrow)
    for (auto *widget = focus; widget && !searching; widget = gtk_widget_get_parent(widget))
      if (GTK_IS_BUTTON(widget)) return FALSE;
  if (modifiers & (GDK_CONTROL_MASK | GDK_ALT_MASK | GDK_SUPER_MASK)) return FALSE;
  if (searching && !arrow) {
    if (keyval == GDK_KEY_Return || keyval == GDK_KEY_KP_Enter) {
      auto selected = gtk_single_selection_get_selected(selection_);
      if (selected != GTK_INVALID_LIST_POSITION) {
        gtk_widget_grab_focus(grid_);
        gtk_grid_view_scroll_to(GTK_GRID_VIEW(grid_), selected, GTK_LIST_SCROLL_FOCUS, nullptr);
      }
      return TRUE;
    }
    return FALSE;
  }
  int delta = 0;
  if (keyval == GDK_KEY_Left || (!searching && keyval == GDK_KEY_h)) delta = -1;
  if (keyval == GDK_KEY_Right || (!searching && keyval == GDK_KEY_l)) delta = 1;
  if (keyval == GDK_KEY_Up || (!searching && keyval == GDK_KEY_k)) delta = -gridColumns_;
  if (keyval == GDK_KEY_Down || (!searching && keyval == GDK_KEY_j)) delta = gridColumns_;
  if (keyval == GDK_KEY_Page_Up) delta = -gridColumns_ * 2;
  if (keyval == GDK_KEY_Page_Down) delta = gridColumns_ * 2;
  if (delta) {
    auto count = g_list_model_get_n_items(G_LIST_MODEL(selection_));
    if (count) {
      auto current = gtk_single_selection_get_selected(selection_);
      int next = ((current == GTK_INVALID_LIST_POSITION ? 0 : int(current)) + delta + int(count) * 20) % int(count);
      gtk_widget_grab_focus(grid_);
      gtk_single_selection_set_selected(selection_, next);
      gtk_grid_view_scroll_to(GTK_GRID_VIEW(grid_), next, GTK_LIST_SCROLL_FOCUS, nullptr);
    }
    return TRUE;
  }
  if (keyval == GDK_KEY_Return || keyval == GDK_KEY_KP_Enter ||
      (!searching && keyval == GDK_KEY_space)) {
    view->apply();
    return TRUE;
  }
  return FALSE;
}
void Gallery::install_actions() {
  auto *keys = gtk_event_controller_key_new();
  g_signal_connect(keys, "key-pressed", G_CALLBACK(key), this);
  gtk_widget_add_controller(GTK_WIDGET(window_), keys);
  g_signal_connect(
      window_, "close-request",
      reinterpret_cast<GCallback>(+[](GtkWindow *, gpointer self) -> gboolean {
        g_cancellable_cancel(static_cast<Gallery *>(self)->cancellation_);
        return FALSE;
      }),
      this);
}

} // namespace anto
