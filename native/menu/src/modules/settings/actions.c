#include "internal.h"

gboolean anto_settings_toggle(GtkSwitch *widget, gboolean state, gpointer data) {
    (void)widget;
    Preference *value = data;
    json_object *object = anto_settings_preferences();
    json_object_object_add(object, value->key, json_object_new_boolean(state));
    g_autoptr(GError) error = NULL;
    gboolean ok = anto_local_config_write_text("wallpaper/preferences.json", json_object_to_json_string_ext(object, JSON_C_TO_STRING_PRETTY), 0600, &error);
    json_object_put(object);
    if (!ok) {menu_notify("Impostazioni", error ? error->message : "Salvataggio non riuscito"); return TRUE;}
    return FALSE;
}
