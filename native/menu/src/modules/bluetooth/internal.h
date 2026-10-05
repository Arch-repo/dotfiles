#pragma once
/* Page-private state and cross-file contract. Public API stays in menu.h. */
#include "menu.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
    gboolean available;
    char *address;
    char *alias;
    gboolean powered;
    gboolean pairable;
    gboolean discoverable;
    gboolean discovering;
    guint paired_count;
    guint connected_count;
    guint seen_count;
} BluetoothStatus;

typedef struct {
    char *address;
    char *alias;
    char *name;
    gboolean powered;
    gboolean pairable;
    gboolean discoverable;
    gboolean discovering;
    gboolean is_default;
} BluetoothController;

typedef struct {
    char *address;
    char *name;
    char *alias;
    char *icon;
    gboolean paired;
    gboolean trusted;
    gboolean blocked;
    gboolean connected;
    int battery;
    int rssi;
} BluetoothDevice;

typedef struct {
    char *profile;
    char *description;
    gboolean available;
    gboolean active;
} BluetoothAudioProfile;

typedef struct {
    BluetoothStatus status;
    GPtrArray *controllers;
    GPtrArray *devices;
    GHashTable *audio_profiles;
} BluetoothSnapshot;

typedef struct {
    MenuApp *app;
    char *operation;
    char *argument;
    char *value;
} BluetoothAction;

typedef struct {
    MenuApp *app;
    char *operation;
} BluetoothSwitchAction;

typedef struct {
    MenuApp *app;
    GSubprocess *process;
    GWeakRef window;
    GWeakRef source;
    GWeakRef scope;
    gboolean pairing;
} BluetoothPending;

typedef struct {
    GtkWidget *container;
    GtkWidget *list;
    guint visible_cards;
} BluetoothGroup;

typedef struct {
    GtkWidget *row;
    GtkWidget *card;
    GtkWidget *icon;
    GtkWidget *name_label;
    GtkWidget *meta_label;
    GtkWidget *address_label;
    GtkWidget *status;
    GtkWidget *battery_label;
    GtkWidget *battery_bar;
    GtkWidget *state;
    GtkWidget *actions;
    GtkWidget *primary_action;
    GtkWidget *trust_action;
    GtkWidget *block_action;
    GtkWidget *remove_action;
    GtkWidget *profile_actions[2];
    GtkWidget *route_action;
    guint group_index;
    char *address;
    char *search_text;
} BluetoothCardRef;

typedef struct {
    GtkWidget *row;
    GtkWidget *icon;
    GtkWidget *name_label;
    GtkWidget *meta_label;
    GtkWidget *state;
    GtkWidget *select_button;
    char *address;
} BluetoothControllerRef;

typedef struct {
    MenuApp *app;
    GPtrArray *groups;
    GPtrArray *cards;
    GHashTable *cards_by_address;
    GPtrArray *controllers;
    GHashTable *controllers_by_address;
    GtkWidget *scroll;
    GtkWidget *shell;
    GtkWidget *hero;
    GtkWidget *hero_icon;
    GtkWidget *hero_title;
    GtkWidget *hero_identity;
    GtkWidget *hero_summary;
    GtkWidget *hero_state;
    GtkWidget *power_switch;
    GtkWidget *controller_section;
    GtkWidget *controller_panel;
    GtkWidget *scan_icon_holder;
    GtkWidget *scan_spinner;
    GtkWidget *scan_icon;
    GtkWidget *scan_title;
    GtkWidget *scan_subtitle;
    GtkWidget *scan_button;
    GtkWidget *discoverable_dot;
    GtkWidget *discoverable_switch;
    GtkWidget *pairable_dot;
    GtkWidget *pairable_switch;
    GtkWidget *adapters_section;
    GtkWidget *adapters_box;
    GtkWidget *unavailable_empty;
    GtkWidget *paused_empty;
    GtkWidget *devices_empty;
    GtkWidget *search_empty;
    guint scroll_restore_id;
    double scroll_value;
    gboolean devices_enabled;
    gboolean initialized;
} BluetoothView;

typedef struct {
    gint refs;
    MenuApp *app;
    GWeakRef window;
    GWeakRef root;
    gboolean in_flight;
    gboolean refresh_pending;
    BluetoothSnapshot *snapshot;
} BluetoothAsyncState;



char *anto_bluetooth_action_path(void);
gboolean anto_bluetooth_text_is_true(const char *text);
const char *anto_bluetooth_present_text(const char *text);
guint anto_bluetooth_parse_count(const char *text);
int anto_bluetooth_parse_number(const char *text, int fallback);
gboolean anto_bluetooth_run_bluetooth(const char *operation, const char *argument,
                              const char *value, char **output, char **error);
char *anto_bluetooth_read_bluetooth(const char *operation, const char *argument);
void anto_bluetooth_status_clear(BluetoothStatus *status);
BluetoothStatus anto_bluetooth_status_parse(const char *output);
void anto_bluetooth_controller_free(gpointer data);
GPtrArray *anto_bluetooth_controllers_parse(const char *output);
void anto_bluetooth_device_free(gpointer data);
GPtrArray *anto_bluetooth_devices_parse(const char *output);
void anto_bluetooth_audio_profile_free(gpointer data);
GPtrArray *anto_bluetooth_audio_profiles_read(const char *address);
void anto_bluetooth_snapshot_free(gpointer data);
void anto_bluetooth_snapshot_load(GTask *task, gpointer source,
                                    gpointer task_data, GCancellable *cancellable);
void anto_bluetooth_action_free(gpointer data, GClosure *closure);
void anto_bluetooth_pending_free(BluetoothPending *pending);
void anto_bluetooth_operation_finished(GObject *object, GAsyncResult *result,
                                         gpointer data);
void anto_bluetooth_operation_start(MenuApp *app, GtkWidget *source,
                                      const char *operation, const char *argument,
                                      const char *value);
void anto_bluetooth_action_clicked(GtkButton *button, gpointer data);
BluetoothAction *anto_bluetooth_action_new(MenuApp *app, const char *operation,
                                             const char *argument, const char *value);
GtkWidget *anto_bluetooth_action_button(MenuApp *app, const char *label,
                                          const char *icon, const char *operation,
                                          const char *argument, const char *value,
                                          const char *state_class);
void anto_bluetooth_manager_clicked(GtkButton *button, gpointer data);
GtkWidget *anto_bluetooth_manager_button(MenuApp *app);
void anto_bluetooth_switch_action_free(gpointer data, GClosure *closure);
void anto_bluetooth_switch_changed(GObject *object, GParamSpec *spec, gpointer data);
GtkWidget *anto_bluetooth_switch(MenuApp *app, const char *operation,
                                   gboolean active, gboolean sensitive,
                                   const char *tooltip);
GtkWidget *anto_bluetooth_section_label(const char *title);
GtkWidget *anto_bluetooth_state_chip(const char *text, const char *state);
void anto_bluetooth_state_chip_update(GtkWidget *chip, const char *text,
                              const char *state);
void anto_bluetooth_switch_update(GtkWidget *control, gboolean active,
                                    gboolean sensitive,
                                    const char *tooltip);
void anto_bluetooth_action_button_update(GtkWidget *button,
                                           const char *label,
                                           const char *icon,
                                           const char *argument,
                                           const char *state_class);
const char *anto_bluetooth_device_display_name(const BluetoothDevice *device);
const char *anto_bluetooth_device_type(const BluetoothDevice *device);
const char *anto_bluetooth_device_icon(const BluetoothDevice *device);
char *anto_bluetooth_device_metadata(const BluetoothDevice *device);
GtkWidget *anto_bluetooth_action_bar_new(void);
void anto_bluetooth_action_bar_append(GtkWidget *bar, GtkWidget *button);
const char *anto_bluetooth_audio_profile_label(const BluetoothAudioProfile *profile);
void anto_bluetooth_action_button_configure(
    GtkWidget *button, const char *label, const char *icon,
    const char *operation, const char *argument, const char *value,
    const char *state_class, gboolean visible, gboolean sensitive);
void anto_bluetooth_device_actions_build_once(MenuApp *app,
                                      BluetoothCardRef *reference,
                                      const BluetoothDevice *device);
void anto_bluetooth_device_actions_update(BluetoothCardRef *reference,
                                  const BluetoothDevice *device,
                                  const GPtrArray *profiles);
void anto_bluetooth_device_status_update(BluetoothCardRef *reference,
                                 const BluetoothDevice *device);
void anto_bluetooth_device_card_update(BluetoothCardRef *reference,
                               const BluetoothDevice *device,
                               const GPtrArray *profiles);
BluetoothCardRef *anto_bluetooth_device_card_new(MenuApp *app,
                                         const BluetoothDevice *device,
                                         const GPtrArray *profiles);
void anto_bluetooth_group_free(gpointer data);
void anto_bluetooth_card_ref_free(gpointer data);
void anto_bluetooth_controller_ref_free(gpointer data);
void anto_bluetooth_view_free(gpointer data);
BluetoothGroup *anto_bluetooth_device_group_append(BluetoothView *view, GtkWidget *shell,
                                           const char *title);
GtkWidget *anto_bluetooth_empty_state(const char *icon, const char *title,
                              const char *subtitle, gboolean spinning);
void anto_bluetooth_search(MenuApp *app, const char *query, gpointer data);
gboolean anto_bluetooth_restore_scroll(gpointer data);
void anto_bluetooth_schedule_scroll_restore(BluetoothView *view,
                                              double value);
GtkWidget *anto_bluetooth_controller_setting_live(MenuApp *app, const char *title,
                                          const char *subtitle,
                                          const char *operation,
                                          GtkWidget **dot_out,
                                          GtkWidget **switch_out);
void anto_bluetooth_build_controller_panel_live(BluetoothView *view);
void anto_bluetooth_build_hero_live(BluetoothView *view);
void anto_bluetooth_status_dot_update(GtkWidget *dot, gboolean active);
void anto_bluetooth_empty_update(GtkWidget *empty, const char *title,
                                   const char *subtitle);
const char *anto_bluetooth_controller_display_name(
    const BluetoothController *controller);
BluetoothControllerRef *anto_bluetooth_controller_ref_new(
    BluetoothView *view, const BluetoothController *controller);
void anto_bluetooth_controller_ref_update(
    BluetoothControllerRef *reference,
    const BluetoothController *controller);
void anto_bluetooth_controllers_reconcile(
    BluetoothView *view, const BluetoothSnapshot *snapshot);
guint anto_bluetooth_device_group(const BluetoothDevice *device);
void anto_bluetooth_device_card_move(BluetoothView *view,
                                       BluetoothCardRef *reference,
                                       guint group_index);
void anto_bluetooth_devices_reconcile(
    BluetoothView *view, const BluetoothSnapshot *snapshot);
void anto_bluetooth_view_apply(BluetoothView *view,
                                 const BluetoothSnapshot *snapshot);
BluetoothAsyncState *anto_bluetooth_async_state_ref(BluetoothAsyncState *state);
void anto_bluetooth_async_state_unref(gpointer data);
BluetoothAsyncState *anto_bluetooth_async_state_get(MenuApp *app);
gboolean anto_bluetooth_root_is_current(BluetoothAsyncState *state);
GtkWidget *anto_bluetooth_scroller_new(GtkWidget **shell_out);
BluetoothView *anto_bluetooth_current_view(
    BluetoothAsyncState *state);
void anto_bluetooth_build_once(MenuApp *app,
                                 BluetoothAsyncState *state);
void anto_bluetooth_snapshot_finished(GObject *object, GAsyncResult *result,
                                        gpointer data);
void anto_bluetooth_refresh_start(BluetoothAsyncState *state);
void menu_show_bluetooth(MenuApp *app);
void menu_bluetooth_live_event(MenuApp *app);
