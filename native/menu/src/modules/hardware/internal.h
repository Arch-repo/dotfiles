#pragma once
/* Page-private state and cross-file contract. Public API stays in menu.h. */
#include "menu.h"
#include "query.h"

#include <string.h>

#define HARDWARE_RUNTIME_KEY "anto-menu-hardware-runtime"

typedef struct {
    gboolean parsed;
    char *cpu;
    char *memory;
    char *disk;
    char *temperature;
    char *kernel;
    char *profile;
} HardwareSnapshot;

typedef struct {
    MenuApp *app;
    AntoQuery *query;
    GSubprocess *profile_process;
    HardwareSnapshot *snapshot;
    GWeakRef root;
    gboolean full_view;
    GtkWidget *cpu_detail;
    GtkWidget *memory_detail;
    GtkWidget *disk_detail;
    GtkWidget *temperature_detail;
    GtkWidget *metric_rows[4];
    GtkWidget *profile_rows[3];
    GtkWidget *profile_badges[3];
} HardwareRuntime;

typedef struct {
    GWeakRef window;
    GSubprocess *process;
} HardwarePending;

typedef struct {
    HardwareRuntime *runtime;
    char *profile;
} HardwareProfileAction;



char *anto_hardware_performance_action_path(void);
void anto_hardware_snapshot_free(HardwareSnapshot *snapshot);
void anto_hardware_runtime_free(gpointer data);
HardwareRuntime *anto_hardware_runtime_get(MenuApp *app);
gboolean anto_hardware_root_is_current(HardwareRuntime *runtime);
HardwareSnapshot *anto_hardware_snapshot_parse(const char *output);
const char *anto_hardware_present(const char *text, const char *fallback);
GtkWidget *anto_hardware_find_css_descendant(GtkWidget *widget,
                                      const char *css_class);
GtkWidget *anto_hardware_add_metric(MenuApp *app, const char *icon,
                                      const char *title, const char *value,
                                      const char *badge,
                                      GtkWidget **detail_out);
gboolean anto_hardware_row_search_set(GtkWidget *row, const char *title,
                                        const char *subtitle,
                                        const char *badge);
void anto_hardware_apply_snapshot(HardwareRuntime *runtime,
                                    const HardwareSnapshot *snapshot);
void anto_hardware_pending_free(HardwarePending *pending);
void anto_hardware_profile_finished(GObject *object, GAsyncResult *result,
                                      gpointer data);
void anto_hardware_profile_action(MenuApp *app, gpointer data);
void anto_hardware_profile_action_free(gpointer data);
GtkWidget *anto_hardware_add_profile(HardwareRuntime *runtime,
                                       const char *icon, const char *title,
                                       const char *subtitle,
                                       const char *profile,
                                       GtkWidget **badge_out);
void anto_hardware_render(HardwareRuntime *runtime,
                            const HardwareSnapshot *snapshot);
void anto_hardware_start_snapshot(MenuApp *app);
void menu_hardware_live_event(MenuApp *app);
void menu_show_hardware(MenuApp *app);
