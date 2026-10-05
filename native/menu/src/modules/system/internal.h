#pragma once
/* Page-private state and cross-file contract. Public API stays in menu.h. */
#include "menu.h"

#include <string.h>

#define SYSTEM_LIVE_KEY "anto-menu-system-live"

typedef struct {
    gboolean valid;
    char *volume;
    char *network;
    char *bluetooth;
    char *brightness;
    char *displays;
} SystemSnapshot;

typedef struct {
    MenuApp *app;
    SystemSnapshot *snapshot;
    GtkWidget *badges[5];
    GtkWidget *tiles[5];
    gboolean mounted;
} SystemLive;




void anto_system_snapshot_free(SystemSnapshot *snapshot);
void anto_system_replace_text(char **target, const char *value);
SystemSnapshot *anto_system_snapshot_parse(const char *output);
const char *anto_system_snapshot_text(const char *text, const char *fallback);
GtkWidget *anto_system_find_widget_with_class(GtkWidget *root,
                                         const char *css_class);
GtkWidget *anto_system_tile_at(MenuApp *app, int index);
gboolean anto_system_tile_search_set(GtkWidget *tile, const char *title,
                                       const char *subtitle,
                                       const char *value);
void anto_system_apply_snapshot(SystemLive *live,
                                  const SystemSnapshot *snapshot);
void anto_system_live_free(gpointer data);
SystemLive *anto_system_live_get(MenuApp *app);
void anto_system_render(SystemLive *live, const SystemSnapshot *snapshot);
void anto_system_refresh_start(SystemLive *live);
void menu_system_live_event(MenuApp *app);
void menu_show_system(MenuApp *app);
