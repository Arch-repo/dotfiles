#include "../gallery.hpp"
#include "../apply.hpp"
#include "primitives.h"
#include <filesystem>
#include <fstream>
#include <vector>

namespace fs = std::filesystem;
static void settle(int milliseconds = 250) {
  gint64 until = g_get_monotonic_time() + milliseconds * 1000;
  while (g_get_monotonic_time() < until) {
    while (g_main_context_iteration(nullptr, FALSE));
    g_usleep(1000);
  }
}
static GtkWidget *find(GtkWidget *root, const char *role) {
  if (gtk_widget_has_css_class(root, role)) return root;
  for (auto *child = gtk_widget_get_first_child(root); child; child = gtk_widget_get_next_sibling(child))
    if (auto *match = find(child, role)) return match;
  return nullptr;
}
static GtkWidget *choice(GtkWidget *root, const char *label) {
  if (GTK_IS_TOGGLE_BUTTON(root) && g_strcmp0(gtk_button_get_label(GTK_BUTTON(root)), label) == 0) return root;
  for (auto *child = gtk_widget_get_first_child(root); child; child = gtk_widget_get_next_sibling(child))
    if (auto *match = choice(child, label)) return match;
  return nullptr;
}
static void executable(const fs::path &path, const char *content) {
  std::ofstream(path) << content;
  fs::permissions(path, fs::perms::owner_all);
}

int main(int argc, char **argv) {
  if (argc == 4 && std::string(argv[1]) == "--apply-worker")
    return anto::run_wallpaper_apply(argv[2], argv[3]);
  g_autofree char *self = g_file_read_link("/proc/self/exe", nullptr);
  g_setenv("ANTO426_WALLPAPER_WORKER", self, TRUE);
  if (!gtk_init_check()) return 77;
  auto root = fs::path(g_get_user_cache_dir()) / "gallery-fixture";
  auto collection = root / "collection";
  fs::create_directories(collection);
  for (const char *name : {"A.png", "B.png", "C.gif"}) {
    auto *pixels = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, 4000, 2250);
    gdk_pixbuf_fill(pixels, 0x6688aaff);
    g_assert_true(gdk_pixbuf_save(pixels, (collection / name).c_str(), "png", nullptr, nullptr));
    g_object_unref(pixels);
  }
  g_setenv("ANTO426_WALLPAPERS_DIR", collection.c_str(), TRUE);
  auto bin = root / "bin";
  fs::create_directories(bin);
  executable(bin / "hyprctl", "#!/bin/sh\nprintf '%s\\n' '[{\"name\":\"eDP-1\",\"focused\":true},{\"name\":\"HDMI-A-1\"}]'\n");
  std::string path = bin.string() + ":" + g_getenv("PATH");
  g_setenv("PATH", path.c_str(), TRUE);
  auto states = fs::path(g_get_user_config_dir()) / "anto426-local/wallpaper/outputs";
  fs::create_directories(states);
  std::ofstream(states / "eDP-1.state") << "eDP-1\nimage\n" << (collection / "B.png").string() << '\n';
  auto *application = gtk_application_new("com.anto426.GalleryFixture", G_APPLICATION_NON_UNIQUE);
  g_assert_true(g_application_register(G_APPLICATION(application), nullptr, nullptr));
  auto gallery = std::make_unique<anto::Gallery>(application);
  gallery->present();
  auto *window = GTK_WINDOW(gtk_application_get_windows(application)->data);
  auto *panel = find(GTK_WIDGET(window), "wallpaper-panel");
  auto *grid = find(panel, "wallpaper-grid");
  auto *preview = find(panel, "wallpaper-preview");
  auto *search = find(panel, "ui-search");
  auto *empty = find(panel, "wallpaper-empty");
  auto *footer = find(panel, "wallpaper-footer");
  auto *apply = gtk_widget_get_last_child(footer);
  auto *selection = GTK_SINGLE_SELECTION(gtk_grid_view_get_model(GTK_GRID_VIEW(grid)));
  for (int i = 0; i < 40 && g_list_model_get_n_items(G_LIST_MODEL(selection)) != 3; i++) settle(50);
  g_assert_cmpuint(g_list_model_get_n_items(G_LIST_MODEL(selection)), ==, 3);
  settle();
  g_assert_cmpstr(gtk_string_object_get_string(GTK_STRING_OBJECT(gtk_single_selection_get_selected_item(selection))), ==, "1");
  g_assert_false(gtk_widget_get_visible(empty));
  g_assert_true(gtk_widget_get_sensitive(apply));
  graphene_rect_t library_bounds, preview_bounds;
  g_assert_true(gtk_widget_compute_bounds(grid, panel, &library_bounds));
  g_assert_true(gtk_widget_compute_bounds(preview, panel, &preview_bounds));
  g_assert_cmpfloat(preview_bounds.origin.x + preview_bounds.size.width, <, library_bounds.origin.x);
  g_assert_cmpfloat(preview_bounds.size.width, >, library_bounds.size.width * 2);
  g_assert_cmpfloat(library_bounds.size.width, <=, ANTO_SIZE_GALLERY_LIBRARY);
  g_assert_cmpuint(gtk_grid_view_get_max_columns(GTK_GRID_VIEW(grid)), ==, 2);
  g_assert_cmpint(gtk_widget_get_width(panel), <=, ANTO_SIZE_GALLERY_WIDTH);
  g_assert_cmpint(gtk_widget_get_height(panel), <=, ANTO_SIZE_GALLERY_HEIGHT);
  gtk_editable_set_text(GTK_EDITABLE(search), "nessun risultato possibile");
  settle();
  g_assert_cmpuint(g_list_model_get_n_items(G_LIST_MODEL(selection)), ==, 0);
  g_assert_true(gtk_widget_get_visible(empty));
  g_assert_false(gtk_widget_get_sensitive(apply));
  gtk_editable_set_text(GTK_EDITABLE(search), "");
  settle();
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(choice(panel, "Live")), TRUE);
  settle();
  g_assert_cmpuint(g_list_model_get_n_items(G_LIST_MODEL(selection)), ==, 1);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(choice(panel, "Immagini")), TRUE);
  settle();
  g_assert_cmpuint(g_list_model_get_n_items(G_LIST_MODEL(selection)), ==, 2);
  g_assert_null(choice(panel, "HDMI-A-1"));
  g_assert_null(find(panel, "wallpaper-targets"));
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(choice(panel, "Tutti")), TRUE);
  settle();
  g_assert_cmpuint(g_list_model_get_n_items(G_LIST_MODEL(selection)), ==, 3);
  gtk_single_selection_set_selected(selection, 2);
  auto result = root / "applied";
  auto release = root / "release";
  g_setenv("ANTO426_GALLERY_RESULT", result.c_str(), TRUE);
  g_setenv("ANTO426_GALLERY_RELEASE", release.c_str(), TRUE);
  executable(bin / "backend", "#!/bin/sh\nfor i in 1 2 3 4 5 6 7 8 9 10; do [ -e \"$ANTO426_GALLERY_RELEASE\" ] && break; sleep .1; done\nprintf '%s\\n' \"$ANTO426_WALLPAPER_OUTPUT\" \"$2\" > \"$ANTO426_GALLERY_RESULT\"\n");
  g_setenv("ANTO426_WALLPAPER_CORE", (bin / "backend").c_str(), TRUE);
  g_object_ref(window);
  g_assert_true(gtk_widget_activate_action(panel, "wallpaper.apply", nullptr));
  g_assert_false(gtk_widget_get_mapped(GTK_WIDGET(window)));
  g_assert_false(fs::exists(result));
  std::ofstream(release).put('\n');
  for (int i = 0; i < 40 && !fs::exists(result); i++) settle(50);
  g_assert_true(fs::exists(result));
  std::ifstream applied(result);
  std::string output;
  std::getline(applied, output);
  g_assert_cmpstr(output.c_str(), ==, "ALL");
  gallery.reset();
  gtk_window_destroy(window);
  g_object_unref(window);
  settle(300); // Pending load/preview completions must not access a destroyed gallery.
  gallery = std::make_unique<anto::Gallery>(application);
  window = GTK_WINDOW(gtk_application_get_windows(application)->data);
  gallery.reset();
  gtk_window_destroy(window);
  settle(300);
  g_object_unref(application);
  g_print("gallery UI: large preview left, two-column library right, bounds, current selection, search, filters, ALL output, background apply and pending-task lifetime verified\n");
}
