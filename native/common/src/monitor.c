#include "monitor.h"
#include <gdk/wayland/gdkwayland.h>
#include <gtk4-layer-shell.h>
#include <json-c/json.h>
#include <string.h>

gboolean anto_layer_shell_supported(GdkDisplay *display) {
    return display && GDK_IS_WAYLAND_DISPLAY(display) && gtk_layer_is_supported();
}

static json_object *query(const char *operation) {
    const char *argv[] = {"timeout", "2", "hyprctl", "-j", operation, NULL};
    g_autoptr(GSubprocess) process = g_subprocess_newv(argv,
        G_SUBPROCESS_FLAGS_STDOUT_PIPE | G_SUBPROCESS_FLAGS_STDERR_SILENCE, NULL);
    g_autofree char *output = NULL;
    if (!process || !g_subprocess_communicate_utf8(process, NULL, NULL, &output, NULL, NULL)) return NULL;
    return output ? json_tokener_parse(output) : NULL;
}
static double number(json_object *object, const char *key) {
    json_object *value = NULL;
    return json_object_object_get_ex(object, key, &value) ? json_object_get_double(value) : 0;
}
GdkMonitor *anto_active_monitor(GdkDisplay *display) {
    GListModel *model = gdk_display_get_monitors(display);
    guint count = g_list_model_get_n_items(model);
    if (!count) return NULL;
    if (count == 1) return g_list_model_get_item(model, 0);
    const char *override = g_getenv("ANTO426_TARGET_MONITOR");
    json_object *monitors = NULL, *cursor = NULL;
    const char *connector = override;
    if (!connector || !*connector) {
        monitors = query("monitors"); cursor = query("cursorpos");
        if (monitors && json_object_is_type(monitors, json_type_array)) {
            for (size_t i = 0; i < json_object_array_length(monitors); i++) {
                json_object *item = json_object_array_get_idx(monitors, i), *name = NULL;
                if (!json_object_object_get_ex(item, "name", &name)) continue;
                if (!connector && number(item, "focused")) connector = json_object_get_string(name);
                double scale = number(item, "scale"); if (scale <= 0) scale = 1;
                gboolean rotated = ((int)number(item, "transform") % 2) != 0;
                double width = number(item, rotated ? "height" : "width") / scale;
                double height = number(item, rotated ? "width" : "height") / scale;
                double x = number(item, "x"), y = number(item, "y");
                if (cursor && number(cursor, "x") >= x && number(cursor, "x") < x + width &&
                    number(cursor, "y") >= y && number(cursor, "y") < y + height) {
                    connector = json_object_get_string(name); break;
                }
            }
        }
    }
    GdkMonitor *result = NULL;
    for (guint i = 0; i < count; i++) {
        GdkMonitor *item = g_list_model_get_item(model, i);
        if (connector && g_strcmp0(gdk_monitor_get_connector(item), connector) == 0) { result = item; break; }
        g_object_unref(item);
    }
    if (cursor) json_object_put(cursor);
    if (monitors) json_object_put(monitors);
    return result ? result : g_list_model_get_item(model, 0);
}
