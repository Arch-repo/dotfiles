#pragma once
#include "catalog.hpp"
#include "gallery_page.h"
#include <gtk/gtk.h>
#include <memory>
#include <string>
namespace anto {
class PreviewBackdrop;
class Gallery {
public:
  explicit Gallery(GtkApplication *application);
  Gallery(GtkWindow *window, GtkWidget *search, int width,
          AntoWallpaperNavigate navigate, gpointer data);
  ~Gallery();
  void present();
  GtkWidget *widget() const { return panel_; }
  void search(const char *query);
  gboolean handle_key(guint key, GdkModifierType modifiers);

private:
  GtkApplication *application_;
  bool embedded_ = false;
  AntoWallpaperNavigate navigate_ = nullptr;
  gpointer navigateData_ = nullptr;
  GtkWindow *window_ = nullptr;
  GtkWidget *hero_ = nullptr, *title_ = nullptr, *metadata_ = nullptr, *kind_ = nullptr,
            *count_ = nullptr, *status_ = nullptr, *apply_ = nullptr,
            *grid_ = nullptr, *search_ = nullptr, *panel_ = nullptr,
            *empty_ = nullptr, *empty_title_ = nullptr,
            *empty_detail_ = nullptr, *empty_lead_ = nullptr;
  GtkWidget *header_ = nullptr;
  GSimpleAction *applyAction_ = nullptr;
  GSimpleAction *bootAction_ = nullptr;
  GtkStringList *model_ = nullptr;
  GtkSingleSelection *selection_ = nullptr;
  GCancellable *cancellation_ = nullptr;
  Catalog catalog_;
  std::string query_;
  int category_ = 0;
  unsigned previewGeneration_ = 0, loadGeneration_ = 0;
  gulong selectionHandler_ = 0;
  std::string previewPath_;
  struct Lifetime { Gallery *view; };
  std::shared_ptr<Lifetime> lifetime_;
  std::unique_ptr<PreviewBackdrop> backdrop_;
  int gridColumns_ = 2;
  void build_window();
  void build_header();
  void build_toolbar();
  void build_content(int panel_width);
  void build_preview(GtkWidget *body, int width);
  void build_footer();
  void install_actions();
  void load();
  void filter();
  void selected();
  void apply(const char *target = "ALL");
  void set_can_apply(bool enabled);
  void choose_folder();
  const Wallpaper *selected_wallpaper() const;
  static void setup(GtkSignalListItemFactory *, GtkListItem *, gpointer);
  static void bind(GtkSignalListItemFactory *, GtkListItem *, gpointer);
  static gboolean key(GtkEventControllerKey *, guint, guint, GdkModifierType,
                      gpointer);
};
} // namespace anto
