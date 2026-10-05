#pragma once
/* Page-private state and cross-file contract. Public API stays in menu.h. */
#include "menu.h"
#include "local_config.h"
#include "virtual_output.h"

#include <json-c/json.h>
#include <math.h>
#include <pango/pangocairo.h>
#include <stdlib.h>
#include <string.h>

#define DISPLAY_RUNTIME_KEY "anto-menu-display-runtime"

typedef struct _DisplayArranger DisplayArranger;
typedef struct _DisplayView DisplayView;

typedef struct {
    char *name;
    char *description;
    char *resolution;
    char *refresh;
    char *scale;
    char *position;
    char *workspace;
    char *mirror;
    int transform;
    int mode_count;
    gboolean enabled;
    gboolean focused;
    gboolean dpms;
    GPtrArray *modes;
} DisplayMonitor;

typedef struct {
    char *text;
    int width;
    int height;
    double rate;
} DisplayMode;

typedef struct {
    char *name;
    char *summary;
} DisplayProfile;

typedef struct {
    GPtrArray *monitors;
    GPtrArray *profiles;
    GPtrArray *virtual_outputs;
    gboolean has_last;
    gboolean partial;
    char *editor;
    char *virtual_error;
} DisplaySnapshot;

typedef struct {
    gint refs;
    MenuApp *app;
    GWeakRef window;
    DisplaySnapshot *snapshot;
    DisplayView *view;
    gboolean in_flight;
    gboolean refresh_pending;
    gboolean snapshot_dirty;
} DisplayRuntime;

typedef struct {
    MenuApp *app;
    GWeakRef window;
    GtkAdjustment *adjustment;
    double value;
} DisplayScrollRestore;

typedef enum {
    DISPLAY_SECTION_CONNECTED,
    DISPLAY_SECTION_VIRTUAL,
    DISPLAY_SECTION_LAYOUT,
    DISPLAY_SECTION_SCALE,
    DISPLAY_SECTION_TRANSFORM,
    DISPLAY_SECTION_MODE,
    DISPLAY_SECTION_PROFILE,
    DISPLAY_SECTION_TOOL,
    DISPLAY_SECTION_COUNT,
} DisplaySection;

typedef struct {
    char *key;
    DisplaySection section;
    char *icon;
    char *title;
    char *subtitle;
    char *badge;
    char *command;
    gboolean close_after;
    gboolean hidden;
} DisplayRowSpec;

typedef struct {
    MenuAction callback;
    MenuApp *app;
    gpointer data;
    GDestroyNotify destroy;
} DisplayCompatAction;

typedef struct {
    char *key;
    DisplaySection section;
    GtkWidget *row;
    GtkWidget *icon;
    GtkWidget *title;
    GtkWidget *subtitle;
    GtkWidget *badge;
    DisplayCompatAction action;
    char *icon_name;
    char *title_text;
    char *subtitle_text;
    char *badge_text;
    char *command;
    char *search_text;
    gboolean close_after;
    gboolean available;
    DisplayRuntime *runtime;
} DisplayRow;

typedef struct {
    char *name;
    char *description;
    char *resolution;
    double x;
    double y;
    double initial_x;
    double initial_y;
    double width;
    double height;
    gboolean focused;
    gboolean internal;
} ArrangeMonitor;

struct _DisplayArranger {
    MenuApp *app;
    GPtrArray *monitors;
    GtkWidget *row;
    GtkWidget *area;
    GtkWidget *title_label;
    GtkWidget *detail_label;
    GtkWidget *selection_label;
    GtkWidget *apply_button;
    GtkWidget *reset_button;
    int selected;
    double view_scale;
    double view_offset_x;
    double view_offset_y;
    double drag_origin_x;
    double drag_origin_y;
    gboolean drag_active;
    gboolean dragged;
    gboolean applying;
    gboolean refresh_pending;
};

struct _DisplayView {
    MenuApp *app;
    DisplayRuntime *runtime;
    DisplayArranger *arranger;
    DisplayRow *loading;
    GtkWidget *arranger_header;
    GtkWidget *section_headers[DISPLAY_SECTION_COUNT];
    GHashTable *rows;
    char *query;
    gboolean ready;
};

extern DisplayArranger *anto_display_active_arranger;
extern const char *const anto_display_section_titles[DISPLAY_SECTION_COUNT];
extern const char *const anto_display_section_keys[DISPLAY_SECTION_COUNT];

char *anto_display_action_path(void);
void anto_display_monitor_free(gpointer data);
void anto_display_mode_free(gpointer data);
void anto_display_profile_free(gpointer data);
void anto_display_snapshot_free(gpointer data);
void anto_display_row_spec_free(gpointer data);
void anto_display_row_free(gpointer data);
void anto_display_view_free(gpointer data);
gboolean anto_display_output_is_internal(const char *name);
const char *anto_display_output_icon(const DisplayMonitor *monitor);
gboolean anto_display_outputs_already_mirrored(GPtrArray *monitors);
char *anto_display_command(const char *action, const char *first,
                             const char *second);
char *anto_display_virtual_backend_path(void);
char *anto_display_virtual_command(const char *const arguments[]);
DisplayRowSpec *anto_display_spec_add(
        GPtrArray *specs, DisplaySection section, const char *key,
        const char *icon, const char *title, const char *subtitle,
        const char *badge, const char *command, gboolean close_after);
void anto_display_virtual_command_finished(GObject *object,
                                             GAsyncResult *result,
                                             gpointer data);
void anto_display_row_run(MenuApp *app, gpointer data);
DisplayRow *anto_display_row_new(MenuApp *app, DisplayRuntime *runtime,
                                   const char *key,
                                   DisplaySection section);
void anto_display_row_update(DisplayRow *row,
                               const DisplayRowSpec *spec);
GtkWidget *anto_display_section_row(MenuApp *app, const char *key,
                                      const char *title);
void anto_display_arrange_monitor_free(gpointer data);
void anto_display_arranger_free(gpointer data);
gboolean anto_display_monitor_logical_geometry(const DisplayMonitor *monitor,
                                         double *x, double *y,
                                         double *width, double *height);
void anto_display_rounded_rectangle(cairo_t *cr, double x, double y,
                              double width, double height, double radius);
void anto_display_set_source_rgba(cairo_t *cr, const GdkRGBA *color, double alpha);
void anto_display_draw_centered_text(cairo_t *cr, const char *text,
                               const char *font, const GdkRGBA *color,
                               double alpha, double x, double y, double width);
void anto_display_arranger_compute_view(DisplayArranger *arranger,
                                  int canvas_width, int canvas_height);
void anto_display_arranger_view_rect(const DisplayArranger *arranger,
                               const ArrangeMonitor *monitor,
                               double *x, double *y,
                               double *width, double *height);
gboolean anto_display_rectangles_overlap(const ArrangeMonitor *left,
                                    const ArrangeMonitor *right);
gboolean anto_display_monitor_overlaps_any(const DisplayArranger *arranger,
                                     guint index);
void anto_display_arranger_draw(GtkDrawingArea *area, cairo_t *cr,
                          int width, int height, gpointer data);
int anto_display_arranger_hit_test(DisplayArranger *arranger, double pointer_x,
                             double pointer_y);
gboolean anto_display_arranger_has_changes(const DisplayArranger *arranger);
void anto_display_arranger_update_controls(DisplayArranger *arranger);
gboolean anto_display_live_refresh_idle(gpointer data);
void anto_display_arranger_flush_deferred_refresh(DisplayArranger *arranger);
gboolean anto_display_ranges_near(double first_start, double first_end,
                            double second_start, double second_end,
                            double tolerance);
void anto_display_consider_snap(double delta, double threshold,
                          double *best_delta, gboolean *found);
void anto_display_arranger_snap_monitor(DisplayArranger *arranger, guint selected);
void anto_display_arranger_resolve_overlaps(DisplayArranger *arranger, guint selected);
void anto_display_arranger_normalize(DisplayArranger *arranger);
void anto_display_arranger_drag_begin(GtkGestureDrag *gesture, double start_x,
                                double start_y, gpointer data);
void anto_display_arranger_drag_update(GtkGestureDrag *gesture, double offset_x,
                                 double offset_y, gpointer data);
void anto_display_arranger_drag_end(GtkGestureDrag *gesture, double offset_x,
                              double offset_y, gpointer data);
void anto_display_arranger_reset_clicked(GtkButton *button, gpointer data);
void anto_display_arranger_apply_clicked(GtkButton *button, gpointer data);
DisplayArranger *anto_display_add_display_arranger(MenuApp *app);
void anto_display_arranger_sync_monitors(DisplayArranger *arranger,
                                   GPtrArray *monitors);
GPtrArray *anto_display_parse_monitors(const char *output);
GPtrArray *anto_display_parse_modes(const char *output);
GPtrArray *anto_display_parse_profiles(const char *output);
gboolean anto_display_read_display_command(const char *script, const char *action,
                                     const char *argument,
                                     GCancellable *cancellable,
                                     char **output, GError **error);
void anto_display_snapshot_load(GTask *task, gpointer source,
                                  gpointer task_data,
                                  GCancellable *cancellable);
DisplayRuntime *anto_display_runtime_ref(DisplayRuntime *runtime);
void anto_display_runtime_unref(gpointer data);
DisplayRuntime *anto_display_runtime_get(MenuApp *app);
const char *anto_display_transform_name(int transform);
char *anto_display_monitor_subtitle(const DisplayMonitor *monitor);
gboolean anto_display_same_mode(const DisplayMode *left, const DisplayMode *right);
void anto_display_add_mode_once(GPtrArray *chosen, DisplayMode *mode);
GPtrArray *anto_display_recommended_modes(GPtrArray *modes,
                                    const DisplayMonitor *monitor);
DisplayMonitor *anto_display_monitor_named(GPtrArray *monitors,
                                             const char *name);
gboolean anto_display_monitor_is_managed_virtual(
        const DisplaySnapshot *snapshot, const DisplayMonitor *monitor);
void anto_display_collect_virtual_specs(GPtrArray *specs,
                                  const DisplaySnapshot *snapshot);
void anto_display_collect_monitor_specs(GPtrArray *specs,
                                  const DisplayMonitor *monitor,
                                  guint active_count);
void anto_display_collect_scale_specs(GPtrArray *specs,
                                const DisplayMonitor *monitor);
void anto_display_collect_transform_specs(GPtrArray *specs,
                                    const DisplayMonitor *monitor);
void anto_display_collect_mode_specs(GPtrArray *specs,
                               const DisplayMonitor *monitor);
void anto_display_collect_profile_specs(GPtrArray *specs,
                                  const DisplaySnapshot *snapshot);
GPtrArray *anto_display_collect_display_specs(const DisplaySnapshot *snapshot);
gboolean anto_display_restore_scroll(gpointer data);
void anto_display_scroll_restore_free(gpointer data);
void anto_display_schedule_scroll_restore(MenuApp *app, double value);
DisplayArranger *anto_display_unsafe_arranger(MenuApp *app);
GtkListBoxRow *anto_display_first_visible_selectable(DisplayView *view);
void anto_display_search(MenuApp *app, const char *query, gpointer data);
DisplayView *anto_display_build_page(MenuApp *app,
                                       DisplayRuntime *runtime);
void anto_display_reorder_rows(DisplayView *view, GPtrArray *specs);
void anto_display_reconcile_rows(DisplayView *view, GPtrArray *specs);
void anto_display_show_error_in_place(DisplayRuntime *runtime,
                                        const char *message);
void anto_display_apply_snapshot_in_place(DisplayRuntime *runtime);
gboolean anto_display_render_if_safe(DisplayRuntime *runtime);
void anto_display_snapshot_finished(GObject *object, GAsyncResult *result,
                                      gpointer data);
void anto_display_refresh_start(DisplayRuntime *runtime);
void menu_display_live_event(MenuApp *app);
void menu_show_display(MenuApp *app);
