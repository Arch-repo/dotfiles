#include "apply.hpp"
#include "catalog.hpp"
#include <string>
#include <unistd.h>
int main(int argc, char **argv) {
  if (argc == 4 && std::string(argv[1]) == "--apply-worker")
    return anto::run_wallpaper_apply(argv[2], argv[3]);
  if (argc >= 3 && std::string(argv[1]) == "--apply") {
    g_autoptr(GError) error = nullptr;
    bool launched = anto::launch_wallpaper_apply(
        argv[2], argc > 3 ? argv[3] : "ALL", &error);
    if (error)
      g_printerr("%s\n", error->message);
    return launched ? 0 : 1;
  }
  if (g_getenv("WAYLAND_DISPLAY"))
    g_setenv("GDK_BACKEND", "wayland", TRUE);
  if (argc > 1 && std::string(argv[1]) == "--random") {
    auto catalog = anto::scan_catalog(anto::wallpaper_directory());
    if (catalog.wallpapers.empty()) {
      g_printerr("La raccolta non contiene sfondi\n");
      return 1;
    }
    auto &item =
        catalog.wallpapers[g_random_int_range(0, catalog.wallpapers.size())];
    g_autoptr(GError) error = nullptr;
    const char *output = g_getenv("ANTO426_WALLPAPER_OUTPUT");
    bool ok = anto::launch_wallpaper_apply(
        item.path, output && *output ? output : "ALL", &error);
    if (error)
      g_printerr("%s\n", error->message);
    return ok ? 0 : 1;
  }
  g_autofree char *self = g_file_read_link("/proc/self/exe", nullptr);
  g_autofree char *directory = self ? g_path_get_dirname(self) : nullptr;
  g_autofree char *menu = directory ? g_build_filename(directory, "anto-menu", nullptr) : nullptr;
  if (menu) execl(menu, menu, "wallpaper", nullptr);
  g_printerr("Impossibile aprire la pagina Sfondi del menu: %s\n", g_strerror(errno));
  return 1;
}
