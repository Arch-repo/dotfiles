#pragma once
/* Page-private state and cross-file contract. Public API stays in menu.h. */
#include "menu.h"
#include "query.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define BRIGHTNESS_LIVE_KEY "anto-menu-brightness-live"

typedef struct {
    gboolean valid;
    gboolean has_brightness;
    double brightness;
    char *capacity;
    char *status;
    char *profile;
} BrightnessSnapshot;

typedef struct {
    MenuApp *app;
    AntoQuery *query;
    BrightnessSnapshot *snapshot;
    GtkWidget *scale;
    GtkWidget *scale_row;
    GtkWidget *scale_detail;
    GtkWidget *profile_rows[3];
    GtkWidget *profile_badges[3];
    guint debounce_source;
    guint interaction_source;
    gboolean mounted;
    gboolean updating_widgets;
    gboolean scale_active;
    gboolean pointer_active;
    GSubprocess *profile_process;
    GCancellable *profile_cancellable;
} BrightnessLive;


typedef struct {
    GWeakRef window;
    GSubprocess *process;
} BrightnessOperation;



gboolean anto_brightness_set_search(GtkWidget *row, const char *text);
void anto_brightness_profile_rows_set_sensitive(BrightnessLive *live,
                                                  gboolean sensitive);
char *anto_brightness_performance_action_path(void);
void anto_brightness_snapshot_free(BrightnessSnapshot *snapshot);
void anto_brightness_replace_text(char **target, const char *value);
BrightnessSnapshot *anto_brightness_snapshot_parse(const char *output);
const char *anto_brightness_snapshot_text(const char *text, const char *fallback);
const char *anto_brightness_battery_status_label(const char *status);
const char *anto_brightness_profile_label(const char *profile);
GtkWidget *anto_brightness_find(GtkWidget *root, const char *css_class,
                                  gboolean find_scale);
GtkWidget *anto_brightness_row_widget(MenuApp *app, int row,
                                        const char *css_class,
                                        gboolean find_scale);
void anto_brightness_live_free(gpointer data);
BrightnessLive *anto_brightness_live_get(MenuApp *app);
void anto_brightness_operation_free(BrightnessOperation *operation);
void anto_brightness_performance_finished(GObject *object, GAsyncResult *result,
                                 gpointer data);
void anto_brightness_set_performance_profile(MenuApp *app, gpointer data);
void anto_brightness_set_brightness(MenuApp *app, double value, gpointer data);
void anto_brightness_scale_pressed(GtkGestureClick *gesture, int n_press,
                                     double x, double y, gpointer data);
void anto_brightness_scale_released(GtkGestureClick *gesture, int n_press,
                                      double x, double y, gpointer data);
char *anto_brightness_subtitle(const BrightnessSnapshot *snapshot);
void anto_brightness_apply_snapshot(BrightnessLive *live,
                                      const BrightnessSnapshot *snapshot);
void anto_brightness_render(BrightnessLive *live,
                              const BrightnessSnapshot *snapshot);
void anto_brightness_refresh_start(BrightnessLive *live);
gboolean anto_brightness_debounce_fire(gpointer data);
gboolean anto_brightness_interaction_fire(gpointer data);
void menu_brightness_live_event(MenuApp *app);
void menu_show_brightness(MenuApp *app);
