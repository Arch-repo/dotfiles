#pragma once
/* Page-private state and cross-file contract. Public API stays in menu.h. */
#include "menu.h"

#include <gio/gdesktopappinfo.h>
#include <string.h>

#define LAUNCHER_VIEW_KEY "anto-launcher-view"

typedef struct {
    GAppInfo *info;
} AppEntry;

typedef struct {
    char *key;
    GAppInfo *info;
    char *title;
    char *subtitle;
    char *icon_key;
} AppRecord;

typedef struct {
    GtkWidget *child;
    GtkWidget *icon;
    GtkWidget *title;
    GtkWidget *subtitle;
    AppEntry *entry;
    char *icon_key;
} AppTile;

typedef struct {
    MenuApp *app;
    GHashTable *tiles;
    guint scroll_restore_id;
    double scroll_value;
} LauncherView;



void anto_launcher_app_entry_free(gpointer data);
void anto_launcher_app_record_free(gpointer data);
void anto_launcher_app_tile_free(gpointer data);
void anto_launcher_view_free(gpointer data);
void anto_launcher_launch_app(MenuApp *app, gpointer data);
const char *anto_launcher_app_icon_name(GAppInfo *info);
void anto_launcher_set_app_icon(GtkWidget *image, GAppInfo *info);
char *anto_launcher_app_identity(GAppInfo *info);
char *anto_launcher_app_description(GAppInfo *info);
char *anto_launcher_app_icon_key(GAppInfo *info);
gint anto_launcher_compare_records(gconstpointer left, gconstpointer right);
GPtrArray *anto_launcher_snapshot(void);
GtkWidget *anto_launcher_last_grid_child(GtkWidget *grid);
AppTile *anto_launcher_capture_app_tile(GtkWidget *child, AppEntry *entry);
void anto_launcher_set_label_text(GtkWidget *label, const char *text);
gboolean anto_launcher_update_search_metadata(AppTile *tile,
                                       const AppRecord *record);
gboolean anto_launcher_update_app_tile(AppTile *tile, const AppRecord *record);
AppTile *anto_launcher_add_app_tile(LauncherView *view,
                             const AppRecord *record);
gboolean anto_launcher_restore_launcher_scroll(gpointer data);
void anto_launcher_schedule_scroll_restore(LauncherView *view, double value);
gboolean anto_launcher_reconcile_launcher_records(LauncherView *view,
                                           GPtrArray *records);
gboolean anto_launcher_reconcile_launcher(LauncherView *view);
void menu_show_launcher(MenuApp *app);
void menu_launcher_live_event(MenuApp *app);
