#include "apply.hpp"
int main(int argc, char** argv) {
    if (argc != 4) return 2;
    if (std::string(argv[1]) == "--apply-worker") return anto::run_wallpaper_apply(argv[2], argv[3]);
    g_autoptr(GError) error = nullptr;
    bool ok = anto::launch_wallpaper_apply(argv[2], argv[3], &error);
    if (error) g_printerr("%s\n", error->message);
    return ok ? 0 : 1;
}
