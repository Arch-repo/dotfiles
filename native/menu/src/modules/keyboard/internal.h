#pragma once
/* Page-private state and cross-file contract. Public API stays in menu.h. */
#include "menu.h"

#include <json-c/json.h>

#define KEYBOARD_RUNTIME_KEY "anto-menu-keyboard-runtime"

typedef struct {
    gboolean valid;
    gboolean fcitx_active;
    char *layout;
    char *keyboard;
} KeyboardSnapshot;

typedef struct {
    MenuApp *app;
    AntoQuery *query;
    KeyboardSnapshot *snapshot;
    GWeakRef root;
    GtkWidget *layout_badges[2];
    GtkWidget *fcitx_badge;
    GtkWidget *layout_rows[2];
    GtkWidget *fcitx_row;
} KeyboardRuntime;


extern const char anto_keyboard_snapshot_command[];

void anto_keyboard_snapshot_free(KeyboardSnapshot *snapshot);
void anto_keyboard_runtime_free(gpointer data);
KeyboardRuntime *anto_keyboard_runtime_get(MenuApp *app);
gboolean anto_keyboard_view_is_current(KeyboardRuntime *runtime);
const char *anto_keyboard_json_string(struct json_object *object, const char *key);
KeyboardSnapshot *anto_keyboard_snapshot_parse(const char *output);
GtkWidget *anto_keyboard_find_css_descendant(GtkWidget *widget,
                                      const char *css_class);
GtkWidget *anto_keyboard_row_badge_at(MenuApp *app, int index);
char *anto_keyboard_subtitle(const KeyboardSnapshot *snapshot);
gboolean anto_keyboard_row_search_set(GtkWidget *row, const char *title,
                                        const char *subtitle,
                                        const char *value,
                                        const char *keyboard);
void anto_keyboard_apply_snapshot(KeyboardRuntime *runtime,
                                    const KeyboardSnapshot *snapshot);
void anto_keyboard_refresh_start(KeyboardRuntime *runtime);
void menu_keyboard_live_event(MenuApp *app);
void menu_show_keyboard(MenuApp *app);
