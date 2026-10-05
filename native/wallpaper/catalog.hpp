#pragma once
#include <string>
#include <vector>
namespace anto {
struct Wallpaper { std::string path, title, thumbnail; bool live = false; bool video = false; };
struct Output { std::string name, label; bool focused = false; };
struct Catalog { std::vector<Wallpaper> wallpapers; std::vector<Output> outputs; std::string current; };
Catalog scan_catalog(const std::string& directory);
std::string preview_path(const Wallpaper& wallpaper);
bool matches(const Wallpaper& wallpaper, const std::string& query, int category);
std::string wallpaper_directory();
}
