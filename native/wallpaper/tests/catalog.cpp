#include "catalog.hpp"
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <filesystem>
#include <fstream>
#include <iostream>
int main() {
    namespace fs = std::filesystem;
    gchar *temporary = g_dir_make_tmp("anto-catalog-XXXXXX", nullptr);
    if (!temporary) return 1;
    fs::path directory(temporary); g_free(temporary);
    fs::create_directories(directory / "images");
    g_setenv("XDG_CACHE_HOME", (directory / "cache").c_str(), TRUE);
    g_setenv("XDG_CONFIG_HOME", (directory / "config").c_str(), TRUE);
    auto *image = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, 16, 12);
    gdk_pixbuf_fill(image, 0x8fabdfff);
    gdk_pixbuf_save(image, (directory / "images/Èstate 'blu'.PNG").c_str(), "png", nullptr, nullptr);
    gdk_pixbuf_save(image, (directory / "images/Animazione.gif").c_str(), "png", nullptr, nullptr);
    g_object_unref(image);
    std::ofstream(directory / "images/ignorami.txt") << "no image";
    auto catalog = anto::scan_catalog((directory / "images").string());
    bool ok = catalog.wallpapers.size() == 2;
    bool live = false, folded = false, preview = false;
    for (const auto &item : catalog.wallpapers) {
        live |= item.live && anto::matches(item, "", 2) && !anto::matches(item, "", 1);
        folded |= anto::matches(item, "èSTATE", 1);
        preview |= !anto::preview_path(item).empty() && !item.thumbnail.empty();
    }
    ok &= live && folded && preview;
    ok &= !anto::matches(catalog.wallpapers.front(), "not found", 0);
    ok &= anto::scan_catalog((directory / "missing").string()).wallpapers.empty();
    fs::remove_all(directory);
    if (!ok) std::cerr << "Catalog filtering, thumbnail or Unicode regression\n";
    return ok ? 0 : 1;
}
