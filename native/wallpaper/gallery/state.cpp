#include "../gallery.hpp"
#include "preview_backdrop.hpp"
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
void Gallery::load() {
  struct Load {
    std::string directory;
    unsigned generation;
    std::weak_ptr<Lifetime> lifetime;
  };
  auto *task = g_task_new(
      nullptr, cancellation_,
      +[](GObject *, GAsyncResult *result, gpointer) {
        auto *request = static_cast<Load *>(g_task_get_task_data(G_TASK(result)));
        auto owner = request->lifetime.lock();
        if (!owner) return;
        auto *view = owner->view;
        GError *error = nullptr;
        auto *catalog = static_cast<Catalog *>(
            g_task_propagate_pointer(G_TASK(result), &error));
        if (error) {
          if (!g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
            gtk_label_set_text(GTK_LABEL(view->status_), error->message);
            gtk_widget_add_css_class(view->status_, "error");
            gtk_widget_set_visible(view->status_, TRUE);
            gtk_label_set_text(GTK_LABEL(view->empty_title_), "Raccolta non disponibile");
            gtk_label_set_text(GTK_LABEL(view->empty_detail_), "Scegli un’altra cartella con Raccolta");
            gtk_widget_set_visible(view->empty_lead_, FALSE);
            gtk_widget_set_visible(view->empty_, TRUE);
          }
          g_error_free(error);
          return;
        }
        if (!catalog)
          return;
        if (request->generation != view->loadGeneration_) {
          delete catalog;
          return;
        }
        view->catalog_ = std::move(*catalog);
        delete catalog;
        view->filter();
        guint selected = gtk_single_selection_get_selected(view->selection_);
        if (selected != GTK_INVALID_LIST_POSITION)
          gtk_grid_view_scroll_to(GTK_GRID_VIEW(view->grid_), selected, GTK_LIST_SCROLL_FOCUS, nullptr);
      },
      nullptr);
  g_task_set_task_data(
      task, new Load{wallpaper_directory(), ++loadGeneration_, lifetime_},
      +[](gpointer data) { delete static_cast<Load *>(data); });
  g_task_run_in_thread(
      task, +[](GTask *job, gpointer, gpointer data, GCancellable *) {
        try {
          g_task_return_pointer(
              job,
              new Catalog(scan_catalog(static_cast<Load *>(data)->directory)),
              +[](gpointer value) { delete static_cast<Catalog *>(value); });
        } catch (const std::exception &error) {
          g_task_return_new_error(job, G_IO_ERROR, G_IO_ERROR_FAILED, "%s",
                                  error.what());
        }
      });
  g_object_unref(task);
}

const Wallpaper *Gallery::selected_wallpaper() const {
  auto selectedIndex = gtk_single_selection_get_selected(selection_);
  if (selectedIndex == GTK_INVALID_LIST_POSITION)
    return nullptr;
  auto *value = gtk_string_list_get_string(model_, selectedIndex);
  if (!value)
    return nullptr;
  size_t index = std::stoul(value);
  return index < catalog_.wallpapers.size() ? &catalog_.wallpapers[index]
                                            : nullptr;
}

void Gallery::filter() {
  if (!model_)
    return;
  std::string old = previewPath_.empty() ? catalog_.current : previewPath_;
  std::vector<std::string> values;
  for (size_t i = 0; i < catalog_.wallpapers.size(); i++)
    if (matches(catalog_.wallpapers[i], query_, category_))
      values.push_back(std::to_string(i));
  std::vector<const char *> strings;
  for (const auto &value : values)
    strings.push_back(value.c_str());
  strings.push_back(nullptr);
  g_signal_handler_block(selection_, selectionHandler_);
  gtk_string_list_splice(model_, 0,
                         g_list_model_get_n_items(G_LIST_MODEL(model_)),
                         strings.data());
  for (guint i = 0; i < values.size(); i++)
    if (catalog_.wallpapers[std::stoul(values[i])].path == old) {
      gtk_single_selection_set_selected(selection_, i);
      break;
    }
  g_signal_handler_unblock(selection_, selectionHandler_);
  std::string count = std::to_string(values.size()) + " visibili · " +
                      std::to_string(catalog_.wallpapers.size()) + " totali";
  gtk_label_set_text(GTK_LABEL(count_), count.c_str());
  set_can_apply(!values.empty());
  selected();
  gtk_spinner_stop(GTK_SPINNER(empty_lead_));
  gtk_widget_set_visible(empty_lead_, FALSE);
  gtk_label_set_text(GTK_LABEL(empty_title_), catalog_.wallpapers.empty() ? "La raccolta è vuota" : "Nessun risultato");
  gtk_label_set_text(GTK_LABEL(empty_detail_), catalog_.wallpapers.empty()
      ? "Scegli la cartella degli sfondi con Raccolta"
      : "Prova un altro nome o cambia filtro");
  gtk_widget_set_visible(empty_, values.empty());
  if (values.empty()) {
    set_can_apply(false);
  }
  gtk_widget_set_visible(status_, FALSE);
}

void Gallery::selected() {
  auto *item = selected_wallpaper();
  if (!item) {
    ++previewGeneration_;
    previewPath_.clear();
    if (backdrop_) backdrop_->hide();
    if (hero_) gtk_picture_set_paintable(GTK_PICTURE(hero_), nullptr);
    if (title_) gtk_label_set_text(GTK_LABEL(title_), "Seleziona uno sfondo");
    if (metadata_) gtk_label_set_text(GTK_LABEL(metadata_), "Nessuno sfondo selezionato");
    if (kind_) gtk_label_set_text(GTK_LABEL(kind_), "—");
    return;
  }
  if (title_) gtk_label_set_text(GTK_LABEL(title_), item->title.c_str());
  if (title_) gtk_widget_set_tooltip_text(title_, item->title.c_str());
  if (kind_) gtk_label_set_text(GTK_LABEL(kind_), item->live ? "LIVE" : "Immagine");
  if (metadata_) gtk_label_set_text(GTK_LABEL(metadata_), item->live
      ? "Anteprima statica · animazione sul desktop"
      : "Lo sfondo verrà applicato a tutti gli schermi");
  if (previewPath_ == item->path) return;
  previewPath_ = item->path;
  if (hero_) gtk_picture_set_filename(GTK_PICTURE(hero_), item->thumbnail.empty() ? nullptr : item->thumbnail.c_str());
  if (backdrop_ && !item->thumbnail.empty()) {
    auto *thumbnail = gdk_texture_new_from_filename(item->thumbnail.c_str(), nullptr);
    if (thumbnail) { backdrop_->show(GDK_PAINTABLE(thumbnail)); g_object_unref(thumbnail); }
  }
  struct Preview {
    Wallpaper wallpaper;
    unsigned generation;
    std::weak_ptr<Lifetime> lifetime;
  };
  auto *task = g_task_new(
      nullptr, cancellation_,
      +[](GObject *, GAsyncResult *result, gpointer) {
        auto *request =
            static_cast<Preview *>(g_task_get_task_data(G_TASK(result)));
        auto owner = request->lifetime.lock();
        if (!owner) return;
        auto *view = owner->view;
        GError *error = nullptr;
        auto *path = static_cast<std::string *>(
            g_task_propagate_pointer(G_TASK(result), &error));
        if (error) {
          g_error_free(error);
          return;
        }
        std::unique_ptr<std::string> owned(path);
        if (!path || request->generation != view->previewGeneration_ ||
            path->empty())
          return;
        auto *texture = gdk_texture_new_from_filename(path->c_str(), nullptr);
        if (texture) {
          if (view->backdrop_) view->backdrop_->show(GDK_PAINTABLE(texture));
          if (view->hero_) gtk_picture_set_paintable(GTK_PICTURE(view->hero_), GDK_PAINTABLE(texture));
          g_object_unref(texture);
        }
      },
      nullptr);
  g_task_set_task_data(
      task, new Preview{*item, ++previewGeneration_, lifetime_},
      +[](gpointer data) { delete static_cast<Preview *>(data); });
  g_task_run_in_thread(
      task, +[](GTask *job, gpointer, gpointer data, GCancellable *) {
        auto *request = static_cast<Preview *>(data);
        g_task_return_pointer(
            job, new std::string(preview_path(request->wallpaper)),
            +[](gpointer value) { delete static_cast<std::string *>(value); });
      });
  g_object_unref(task);
}
} // namespace anto
