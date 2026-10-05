#include "internal.h"

void anto_settings_preference_free(gpointer data) {Preference *value = data; g_free(value->key); g_free(value);}

json_object *anto_settings_preferences(void) {
    g_autofree char *path = anto_local_config_path("wallpaper/preferences.json", NULL);
    json_object *object = path ? json_object_from_file(path) : NULL;
    if (object && !json_object_is_type(object, json_type_object)) {json_object_put(object); object = NULL;}
    return object ? object : json_object_new_object();
}
