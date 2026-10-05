#include "local_config.h"
#include <json-c/json.h>
void wallpaper_load_preferences(void) {
    g_autofree char *path = anto_local_config_path("wallpaper/preferences.json", NULL);
    json_object *object = path ? json_object_from_file(path) : NULL;
    if (!object || !json_object_is_type(object, json_type_object)) {if (object) json_object_put(object); return;}
    const char *keys[] = {"apps", "vscode", "icons", "boot"};
    const char *variables[] = {"ANTO426_WALLPAPER_CORE_APPS", "ANTO426_WALLPAPER_CORE_VSCODE", "ANTO426_WALLPAPER_CORE_ICONS", "ANTO426_WALLPAPER_CORE_BOOT"};
    for (unsigned i = 0; i < G_N_ELEMENTS(keys); i++) {
        json_object *value = NULL;
        if (!g_getenv(variables[i]) && json_object_object_get_ex(object, keys[i], &value) && json_object_is_type(value, json_type_boolean))
            g_setenv(variables[i], json_object_get_boolean(value) ? "1" : "0", FALSE);
    }
    json_object_put(object);
}
