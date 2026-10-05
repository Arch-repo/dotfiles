#include "local_config.h"
#include "widgets.h"
#include <string.h>

int widget_kind(const char *id) {
  for (guint i = 0; i < widget_definition_count; i++) {
    const WidgetDefinition *def = widget_definitions[i];
    if (g_strcmp0(id, def->id) == 0)
      return i;
    g_auto(GStrv) aliases = g_strsplit(def->legacy_ids, " ", -1);
    for (guint j = 0; aliases[j]; j++)
      if (*aliases[j] && g_strcmp0(id, aliases[j]) == 0)
        return i;
  }
  return -1;
}
/* Import values as data. Legacy command strings are never executed. */
static char *env_value(const char *relative, const char *key) {
  g_autofree char *text = anto_local_config_read_text(relative, NULL, NULL);
  if (!text)
    return NULL;
  g_auto(GStrv) lines = g_strsplit(text, "\n", -1);
  for (guint i = 0; lines[i]; i++) {
    char *line = g_strstrip(lines[i]);
    if (g_str_has_prefix(line, "export "))
      line = g_strstrip(line + 7);
    char *equal = strchr(line, '=');
    if (!equal)
      continue;
    *equal = 0;
    if (g_strcmp0(g_strstrip(line), key))
      continue;
    char *value = g_strstrip(equal + 1);
    gsize length = strlen(value);
    if (length >= 2 && ((*value == '"' && value[length - 1] == '"') ||
                        (*value == '\'' && value[length - 1] == '\'')))
      return g_strndup(value + 1, length - 2);
    return g_strdup(value);
  }
  return NULL;
}
static gboolean legacy_bool(const char *key, gboolean fallback) {
  g_autofree char *value = env_value("widgets/settings.env", key);
  return value ? (!g_strcmp0(value, "1") || !g_ascii_strcasecmp(value, "true"))
               : fallback;
}
static void import_ids(WidgetSettings *s, const char *relative,
                       const char *key) {
  g_autofree char *value = env_value(relative, key);
  g_auto(GStrv) ids = g_strsplit_set(value ? value : "", " \t", -1);
  for (guint i = 0; ids[i]; i++) {
    int kind = widget_kind(ids[i]);
    if (kind >= 0)
      s->visible[kind] = TRUE;
  }
}
static char *unhex(const char *value, gsize length) {
  if (length % 2)
    return NULL;
  char *result = g_malloc0(length / 2 + 1);
  for (gsize i = 0; i < length; i += 2) {
    int a = g_ascii_xdigit_value(value[i]),
        b = g_ascii_xdigit_value(value[i + 1]);
    if (a < 0 || b < 0 || !(a * 16 + b)) {
      g_free(result);
      return NULL;
    }
    result[i / 2] = (char)(a * 16 + b);
  }
  if (!g_utf8_validate(result, -1, NULL)) {
    g_free(result);
    return NULL;
  }
  return result;
}
static void import_layout(WidgetSettings *s) {
  g_autofree char *text =
      anto_local_config_read_text("widgets/layout.env", NULL, NULL);
  if (!text)
    return;
  g_auto(GStrv) lines = g_strsplit(text, "\n", -1);
  for (guint i = 0; lines[i]; i++) {
    char *key = g_strstrip(lines[i]);
    if (g_str_has_prefix(key, "export "))
      key += 7;
    if (!g_str_has_prefix(key, "LAYOUT_W"))
      continue;
    char *output = strstr(key + 8, "_O"),
         *field = output ? strchr(output + 2, '_') : NULL;
    char *equal = field ? strchr(field, '=') : NULL;
    if (!equal || field + 2 != equal)
      continue;
    g_autofree char *id = unhex(key + 8, output - key - 8);
    g_autofree char *connector = unhex(output + 2, field - output - 2);
    int kind = widget_kind(id);
    if (kind < 0 || !connector)
      continue;
    char *value = g_strstrip(equal + 1);
    if (*value == '"')
      value++;
    int number = (int)g_ascii_strtoll(value, NULL, 10);
    json_object *screen = NULL, *item = NULL;
    if (!json_object_object_get_ex(s->layouts, connector, &screen)) {
      screen = json_object_new_object();
      json_object_object_add(s->layouts, connector, screen);
    }
    const char *native_id = widget_definitions[kind]->id;
    if (!json_object_object_get_ex(screen, native_id, &item)) {
      item = json_object_new_object();
      json_object_object_add(screen, native_id, item);
    }
    const char *name = field[1] == 'X'   ? "x"
                       : field[1] == 'Y' ? "y"
                       : field[1] == 'W' ? "width"
                       : field[1] == 'H' ? "height"
                                         : NULL;
    if (name)
      json_object_object_add(item, name, json_object_new_int(number));
  }
}
static gboolean bool_member(json_object *root, const char *key,
                            gboolean fallback) {
  json_object *value = NULL;
  return json_object_object_get_ex(root, key, &value) &&
                 json_object_is_type(value, json_type_boolean)
             ? json_object_get_boolean(value)
             : fallback;
}
gboolean widget_settings_load(WidgetSettings *s, GError **error) {
  memset(s, 0, sizeof(*s));
  s->autostart = TRUE;
  s->locked = TRUE;
  s->layouts = json_object_new_object();
  g_autofree char *path = anto_local_config_path("widgets/state.json", error);
  if (!path)
    return FALSE;
  if (g_file_test(path, G_FILE_TEST_EXISTS)) {
    json_object *root = json_object_from_file(path), *items = NULL,
                *layouts = NULL;
    if (!root || !json_object_is_type(root, json_type_object)) {
      if (root)
        json_object_put(root);
      g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA,
                          "Configurazione widget non valida");
      return FALSE;
    }
    json_object *version = NULL;
    json_object_object_get_ex(root, "version", &version);
    int schema = version ? json_object_get_int(version) : 1;
    if (schema > 2 || schema < 1) {
      json_object_put(root);
      g_set_error_literal(
          error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA,
          "Versione della configurazione widget non supportata");
      return FALSE;
    }
    s->enabled = bool_member(root, "enabled", FALSE);
    s->autostart = bool_member(root, "autostart", TRUE);
    s->locked = bool_member(root, "locked", TRUE);
    if (json_object_object_get_ex(root, "widgets", &items) &&
        json_object_is_type(items, json_type_object))
      for (guint i = 0; i < widget_definition_count; i++)
        s->visible[i] = bool_member(items, widget_definitions[i]->id, FALSE);
    /* v1 had separate media/spectrum cards; either enables the merged card. */
    if (schema == 1 && items) {
      int music = widget_kind("media");
      if (music >= 0)
        s->visible[music] |= bool_member(items, "spectrum", FALSE);
    }
    if (json_object_object_get_ex(root, "layouts", &layouts) &&
        json_object_is_type(layouts, json_type_object)) {
      json_object_put(s->layouts);
      s->layouts = json_object_get(layouts);
    }
    json_object_put(root);
    return schema == 1 ? widget_settings_save(s, error) : TRUE;
  }
  s->enabled = legacy_bool("ANTO426_WIDGETS_MASTER_ENABLED", FALSE);
  s->autostart = legacy_bool("ANTO426_WIDGETS_AUTOSTART", TRUE);
  s->locked = legacy_bool("ANTO426_WIDGETS_LOCKED", TRUE);
  import_ids(s, "widgets/settings.env", "ANTO426_WIDGETS_ENABLED");
  import_ids(s, "widgets/custom.env", "ANTO426_CUSTOM_WIDGETS");
  gboolean any = FALSE;
  for (guint i = 0; i < widget_definition_count; i++)
    any |= s->visible[i];
  if (!any)
    for (guint i = 0; i < widget_definition_count; i++)
      s->visible[i] = TRUE;
  /* A native media card replaces the old Spotify application launcher. */
  int media = widget_kind("media");
  if (media >= 0)
    s->visible[media] = TRUE;
  import_layout(s);
  return widget_settings_save(s, error);
}
gboolean widget_settings_save(WidgetSettings *s, GError **error) {
  int lock = anto_local_config_lock("widgets/state.lock", TRUE, error);
  if (lock < 0)
    return FALSE;
  json_object *root = json_object_new_object(),
              *items = json_object_new_object();
  json_object_object_add(root, "version", json_object_new_int(2));
  json_object_object_add(root, "enabled", json_object_new_boolean(s->enabled));
  json_object_object_add(root, "autostart",
                         json_object_new_boolean(s->autostart));
  json_object_object_add(root, "locked", json_object_new_boolean(s->locked));
  for (guint i = 0; i < widget_definition_count; i++)
    json_object_object_add(items, widget_definitions[i]->id,
                           json_object_new_boolean(s->visible[i]));
  json_object_object_add(root, "widgets", items);
  json_object_object_add(root, "layouts", json_object_get(s->layouts));
  gboolean ok = anto_local_config_write_text(
      "widgets/state.json",
      json_object_to_json_string_ext(root, JSON_C_TO_STRING_PRETTY), 0600,
      error);
  json_object_put(root);
  anto_local_config_unlock(lock);
  return ok;
}
void widget_settings_clear(WidgetSettings *s) {
  if (s->layouts)
    json_object_put(s->layouts);
  memset(s, 0, sizeof(*s));
}
static int integer(json_object *item, const char *key, int fallback) {
  json_object *value = NULL;
  return item && json_object_object_get_ex(item, key, &value) &&
                 json_object_is_type(value, json_type_int)
             ? json_object_get_int(value)
             : fallback;
}
WidgetGeometry widget_geometry(WidgetSettings *s, WidgetKind kind,
                               const char *output, int width, int height) {
  const WidgetDefinition *def = widget_definitions[kind];
  WidgetGeometry g = {
      def->right ? width - def->width - ANTO_SPACING_XXL : ANTO_SPACING_XXL,
      def->bottom ? height - def->height - ANTO_SPACING_XXL : def->default_y,
      def->width, def->height};
  json_object *screen = NULL, *item = NULL;
  if (json_object_object_get_ex(s->layouts, output, &screen))
    json_object_object_get_ex(screen, def->id, &item);
  g.width =
      CLAMP(integer(item, "width", g.width), MIN(280, width), MAX(1, width));
  g.height = CLAMP(integer(item, "height", g.height), MIN(def->height, height),
                   MAX(1, height));
  g.x = CLAMP(integer(item, "x", g.x), 0, MAX(0, width - g.width));
  g.y = CLAMP(integer(item, "y", g.y), 0, MAX(0, height - g.height));
  return g;
}
void widget_geometry_save(WidgetSettings *s, WidgetKind kind,
                          const char *output, WidgetGeometry g) {
  json_object *screen = NULL;
  if (!json_object_object_get_ex(s->layouts, output, &screen)) {
    screen = json_object_new_object();
    json_object_object_add(s->layouts, output, screen);
  }
  json_object *item = json_object_new_object();
  json_object_object_add(item, "x", json_object_new_int(g.x));
  json_object_object_add(item, "y", json_object_new_int(g.y));
  json_object_object_add(item, "width", json_object_new_int(g.width));
  json_object_object_add(item, "height", json_object_new_int(g.height));
  json_object_object_add(screen, widget_definitions[kind]->id, item);
}
