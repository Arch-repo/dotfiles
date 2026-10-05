#pragma once
/* Page-private state and cross-file contract. Public API stays in menu.h. */
#include "menu.h"
#include "local_config.h"
#include <json-c/json.h>

typedef struct { MenuApp *app; char *key; } Preference;



void anto_settings_preference_free(gpointer data);
json_object *anto_settings_preferences(void);
gboolean anto_settings_toggle(GtkSwitch *widget, gboolean state, gpointer data);
void anto_settings_add_preference(MenuApp *app, const char *title, const char *subtitle, const char *key, gboolean fallback, json_object *preferences);
void menu_show_settings(MenuApp *app);
