#pragma once
/* Page-private state and cross-file contract. Public API stays in menu.h. */
#include "menu.h"

#include <json-c/json.h>

#define FLOATING_RUNTIME_KEY "anto-menu-floating-runtime"

typedef struct {
    gboolean valid;
    gboolean floating;
    gboolean pinned;
    gboolean fullscreen;
    int workspace;
    char *address;
    char *class_name;
    char *title;
} FloatingSnapshot;

typedef struct {
    MenuApp *app;
    AntoQuery *query;
    FloatingSnapshot *snapshot;
    GWeakRef root;
    GtkWidget *floating_badge;
    GtkWidget *pin_badge;
} FloatingRuntime;




void anto_floating_snapshot_free(FloatingSnapshot *snapshot);
const char *anto_floating_json_string(struct json_object *object, const char *key);
gboolean anto_floating_json_boolean(struct json_object *object, const char *key);
FloatingSnapshot *anto_floating_snapshot_parse(const char *output);
void anto_floating_runtime_free(gpointer data);
FloatingRuntime *anto_floating_runtime_get(MenuApp *app);
gboolean anto_floating_view_is_current(FloatingRuntime *runtime);
GtkWidget *anto_floating_find_css_descendant(GtkWidget *widget,
                                      const char *css_class);
GtkWidget *anto_floating_tile_badge_at(MenuApp *app, int index);
char *anto_floating_subtitle(const FloatingSnapshot *snapshot);
void anto_floating_apply_snapshot(FloatingRuntime *runtime,
                                    const FloatingSnapshot *snapshot);
void anto_floating_refresh_start(FloatingRuntime *runtime);
void menu_floating_live_event(MenuApp *app);
void menu_show_floating(MenuApp *app);
