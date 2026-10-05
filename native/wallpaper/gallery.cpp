#include "gallery.hpp"
#include "frontend.h"
#include "primitives.h"
#include "gallery/preview_backdrop.hpp"
namespace anto {
Gallery::Gallery(GtkApplication *application) : application_(application) {
  lifetime_ = std::make_shared<Lifetime>(Lifetime{this});
  cancellation_ = g_cancellable_new();
  build_window();
  load();
}
Gallery::Gallery(GtkWindow *window, GtkWidget *search, int width,
                 AntoWallpaperNavigate navigate, gpointer data)
    : application_(gtk_window_get_application(window)), embedded_(true),
      navigate_(navigate), navigateData_(data), window_(window), search_(search) {
  lifetime_ = std::make_shared<Lifetime>(Lifetime{this});
  cancellation_ = g_cancellable_new();
  panel_ = anto_ui_stack(GTK_ORIENTATION_VERTICAL, ANTO_SPACING_MD,
                         "wallpaper-page");
  gtk_widget_set_vexpand(panel_, TRUE);
  gtk_widget_set_overflow(panel_, GTK_OVERFLOW_HIDDEN);
  backdrop_ = std::make_unique<PreviewBackdrop>(window_);
  build_header();
  build_toolbar();
  build_content(width);
  build_footer();
  load();
}
Gallery::~Gallery() {
  lifetime_.reset();
  g_cancellable_cancel(cancellation_);
  g_clear_object(&cancellation_);
  g_clear_object(&model_);
  g_clear_object(&selection_);
}
void Gallery::search(const char *query) {
  query_ = query ? query : "";
  filter();
}
void Gallery::present() {
  anto_frontend_present(window_);
  gtk_widget_grab_focus(grid_);
}
} // namespace anto

GtkWidget *anto_wallpaper_page_new(GtkWindow *window, GtkWidget *search,
                                   int width, AntoWallpaperNavigate navigate,
                                   gpointer data) {
  auto *view = new anto::Gallery(window, search, width, navigate, data);
  auto *page = view->widget();
  g_object_set_data_full(G_OBJECT(page), "gallery-view", view,
                         +[](gpointer owner) { delete static_cast<anto::Gallery *>(owner); });
  return page;
}
void anto_wallpaper_page_search(GtkWidget *page, const char *query) {
  auto *view = static_cast<anto::Gallery *>(g_object_get_data(G_OBJECT(page), "gallery-view"));
  if (view) view->search(query);
}
gboolean anto_wallpaper_page_key(GtkWidget *page, guint key,
                                GdkModifierType modifiers) {
  auto *view = static_cast<anto::Gallery *>(g_object_get_data(G_OBJECT(page), "gallery-view"));
  return view && view->handle_key(key, modifiers);
}
