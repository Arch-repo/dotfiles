#include "menu.h"
#include "virtual_output.h"
#include "workspace_output.h"

#include <stdlib.h>
#include <string.h>

static int run_shell(const char *command) {
    g_autoptr(GError) error = NULL;
    char *argv[] = {"/bin/sh", "-lc", (char *)command, NULL};
    return g_spawn_async(NULL, argv, NULL, G_SPAWN_SEARCH_PATH,
                         NULL, NULL, NULL, &error) ? 0 : 1;
}

int menu_run_direct_action(int argc, char **argv) {
    if (argc < 2) return -1;
    int virtual_output = menu_virtual_output_action(argc - 1, argv + 1);
    if (virtual_output >= 0) return virtual_output;
    int workspace_output =
        menu_workspace_output_action(argc - 1, argv + 1);
    if (workspace_output >= 0) return workspace_output;

    const char *action = argv[1];
    if (g_strcmp0(action, "brightness-up") == 0)
        return run_shell("brightnessctl set 5%+ >/dev/null");
    if (g_strcmp0(action, "brightness-down") == 0)
        return run_shell("brightnessctl set 5%- >/dev/null");
    if (g_strcmp0(action, "capture-area-now") == 0)
        return run_shell("mkdir -p ~/Pictures/Screenshots; hyprshot -m region -o ~/Pictures/Screenshots");
    if (g_strcmp0(action, "capture-window-now") == 0)
        return run_shell("mkdir -p ~/Pictures/Screenshots; hyprshot -m window -o ~/Pictures/Screenshots");
    if (g_strcmp0(action, "capture-monitor-now") == 0)
        return run_shell("mkdir -p ~/Pictures/Screenshots; hyprshot -m output -m active -o ~/Pictures/Screenshots");
    if (g_strcmp0(action, "floating-toggle") == 0)
        return run_shell("hyprctl dispatch togglefloating >/dev/null");
    if (g_strcmp0(action, "floating-center") == 0)
        return run_shell("hyprctl dispatch centerwindow >/dev/null");
    if (g_strcmp0(action, "floating-pin") == 0)
        return run_shell("hyprctl dispatch pin >/dev/null");
    if (g_strcmp0(action, "floating-top") == 0)
        return run_shell("hyprctl dispatch alterzorder top >/dev/null");
    if (g_strcmp0(action, "floating-reset") == 0)
        return run_shell("hyprctl dispatch pin off >/dev/null; hyprctl dispatch settiled >/dev/null");
    if (g_strcmp0(action, "floating-small") == 0)
        return run_shell("hyprctl dispatch resizeactive exact 640 420 >/dev/null; hyprctl dispatch centerwindow >/dev/null");
    if (g_strcmp0(action, "floating-medium") == 0)
        return run_shell("hyprctl dispatch resizeactive exact 920 640 >/dev/null; hyprctl dispatch centerwindow >/dev/null");
    if (g_strcmp0(action, "floating-large") == 0)
        return run_shell("hyprctl dispatch resizeactive exact 1280 820 >/dev/null; hyprctl dispatch centerwindow >/dev/null");

    const char *direction = NULL;
    if (g_str_has_prefix(action, "floating-move-")) direction = action + strlen("floating-move-");
    if (direction) {
        const char *short_direction = direction;
        if (g_strcmp0(direction, "left") == 0) short_direction = "l";
        else if (g_strcmp0(direction, "right") == 0) short_direction = "r";
        else if (g_strcmp0(direction, "up") == 0) short_direction = "u";
        else if (g_strcmp0(direction, "down") == 0) short_direction = "d";
        g_autofree char *command = g_strdup_printf("hyprctl dispatch movewindow %s >/dev/null", short_direction);
        return run_shell(command);
    }
    if (g_str_has_prefix(action, "floating-resize-")) {
        direction = action + strlen("floating-resize-");
        int x = 0, y = 0;
        if (g_strcmp0(direction, "left") == 0) x = -40;
        else if (g_strcmp0(direction, "right") == 0) x = 40;
        else if (g_strcmp0(direction, "up") == 0) y = -40;
        else if (g_strcmp0(direction, "down") == 0) y = 40;
        g_autofree char *command = g_strdup_printf("hyprctl dispatch resizeactive %d %d >/dev/null", x, y);
        return run_shell(command);
    }
    return -1;
}
