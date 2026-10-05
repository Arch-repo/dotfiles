#pragma once
#include <gio/gio.h>
#include <string>

namespace anto {
bool launch_wallpaper_apply(const std::string &path, const std::string &target,
                            GError **error);
int run_wallpaper_apply(const std::string &path, const std::string &target);
} // namespace anto
