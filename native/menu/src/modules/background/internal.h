#pragma once
/* Page-private state and cross-file contract. Public API stays in menu.h. */
#include "menu.h"

#include <json-c/json.h>

#define BACKGROUND_RUNTIME_KEY "anto-menu-background-runtime"

typedef struct {
    char *address;
} WindowEntry;

typedef struct {
    char *address;
    char *title;
    char *class_name;
    int workspace;
} BackgroundWindow;

typedef struct {
    gboolean valid;
    char *cpu;
    char *memory;
    GPtrArray *windows;
} BackgroundSnapshot;

typedef struct {
    GtkWidget *row;
    GtkWidget *title;
    GtkWidget *subtitle;
} BackgroundRow;

typedef struct {
    MenuApp *app;
    AntoQuery *query;
    BackgroundSnapshot *snapshot;
    GHashTable *rows;
    GWeakRef root;
    GtkWidget *empty_row;
    guint scroll_restore_id;
    double scroll_value;
    int rows_start_index;
} BackgroundRuntime;


extern const char anto_background_snapshot_command[];

void anto_background_window_entry_free(gpointer data);
void anto_background_window_free(gpointer data);
void anto_background_snapshot_free(BackgroundSnapshot *snapshot);
void anto_background_row_free(gpointer data);
void anto_background_runtime_free(gpointer data);
BackgroundRuntime *anto_background_runtime_get(MenuApp *app);
gboolean anto_background_view_is_current(BackgroundRuntime *runtime);
const char *anto_background_json_string(struct json_object *object, const char *key);
char *anto_background_metric_from_header(const char *header, const char *key);
BackgroundSnapshot *anto_background_snapshot_parse(const char *output);
void anto_background_focus_window(MenuApp *app, gpointer data);
GtkWidget *anto_background_find_css_descendant(GtkWidget *widget,
                                      const char *css_class);
void anto_background_update_search_data(GtkWidget *row, const char *title,
                               const char *subtitle);
char *anto_background_window_detail(const BackgroundWindow *window);
BackgroundRow *anto_background_add_window(
    BackgroundRuntime *runtime, const BackgroundWindow *window);
void anto_background_update_window(BackgroundRow *binding,
                                     const BackgroundWindow *window);
void anto_background_remove_empty(BackgroundRuntime *runtime);
void anto_background_add_empty(BackgroundRuntime *runtime);
gboolean anto_background_restore_scroll(gpointer data);
void anto_background_schedule_scroll(BackgroundRuntime *runtime,
                                       double value);
void anto_background_apply_snapshot(BackgroundRuntime *runtime,
                                      const BackgroundSnapshot *snapshot);
void anto_background_refresh_start(BackgroundRuntime *runtime);
void menu_background_live_event(MenuApp *app);
void menu_show_background(MenuApp *app);
