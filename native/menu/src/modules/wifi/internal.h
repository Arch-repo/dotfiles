#pragma once
/* Page-private state and cross-file contract. Public API stays in menu.h. */
#include "menu.h"
#include "query.h"

#include <stdlib.h>
#include <string.h>

#define WIFI_RUNTIME_KEY "anto-menu-wifi-runtime"

enum {
    WIFI_GROUP_CONNECTED,
    WIFI_GROUP_SAVED,
    WIFI_GROUP_AVAILABLE,
    WIFI_GROUP_COUNT
};

typedef struct {
    char *ssid;
    char *security;
    char *bssid;
    int signal;
    int frequency;
    int channel;
    gboolean active;
    gboolean saved;
} WifiNetwork;

typedef struct {
    gboolean parsed;
    gboolean available;
    gboolean radio_enabled;
    char *state;
    char *device;
    char *ssid;
    char *ipv4;
    char *gateway;
    char *dns;
    char *connectivity;
    GPtrArray *networks;
} WifiSnapshot;

typedef struct {
    GtkWidget *container;
    GtkWidget *header;
    guint visible_cards;
} WifiGroup;

typedef struct {
    GtkWidget *card;
    GtkWidget *icon;
    GtkWidget *name;
    GtkWidget *saved_chip;
    GtkWidget *meta;
    GtkWidget *signal_label;
    GtkWidget *signal_bar;
    GtkWidget *connect_button;
    guint group_index;
    char *key;
    char *search_text;
} WifiCardRef;

typedef struct {
    MenuApp *app;
    GtkWidget *scroll;
    GtkWidget *shell;
    GtkWidget *hero;
    GtkWidget *hero_icon;
    GtkWidget *hero_title;
    GtkWidget *hero_subtitle;
    GtkWidget *state_chip;
    GtkWidget *radio_switch;
    gulong radio_handler;
    GtkWidget *metrics;
    GtkWidget *ipv4;
    GtkWidget *gateway;
    GtkWidget *dns;
    GtkWidget *refresh_button;
    GtkWidget *disconnect_button;
    GtkWidget *unavailable_empty;
    GtkWidget *radio_empty;
    GtkWidget *networks_empty;
    GPtrArray *groups;
    GPtrArray *cards;
    GtkWidget *search_empty;
    guint scroll_restore_id;
    double scroll_value;
    gboolean networks_enabled;
} WifiView;

typedef struct {
    MenuApp *app;
    AntoQuery *query;
    WifiSnapshot *snapshot;
    guint debounce_source;
} WifiRuntime;


typedef struct {
    GWeakRef window;
    GWeakRef source;
    GWeakRef status;
    GSubprocess *process;
    char *input;
    gboolean return_to_wifi;
} WifiRequest;

typedef struct {
    MenuApp *app;
    char *operation;
    char *argument;
} WifiAction;

typedef struct {
    MenuApp *app;
    GtkWidget *entry;
    GtkWidget *submit;
    GtkWidget *status;
    char *ssid;
} WifiPassword;



char *anto_wifi_network_action_path(void);
gboolean anto_wifi_text_is_true(const char *text);
const char *anto_wifi_present_text(const char *text);
char *anto_wifi_network_key(const char *ssid, const char *bssid);
char *anto_wifi_unescape_field(const char *text);
int anto_wifi_parse_number(const char *text, int fallback);
void anto_wifi_network_free(gpointer data);
void anto_wifi_snapshot_free(WifiSnapshot *snapshot);
WifiSnapshot *anto_wifi_snapshot_new(void);
WifiSnapshot *anto_wifi_snapshot_parse(const char *output);
void anto_wifi_runtime_free(gpointer data);
WifiRuntime *anto_wifi_runtime_get(MenuApp *app);
GtkWidget *anto_wifi_section_label(const char *text);
GtkWidget *anto_wifi_state_chip(const char *text, const char *state);
const char *anto_wifi_signal_icon(int signal);
const char *anto_wifi_connectivity_label(const char *state);
const char *anto_wifi_connection_state_label(const char *state);
const char *anto_wifi_network_band(const WifiNetwork *network);
void anto_wifi_action_free(gpointer data, GClosure *closure);
void anto_wifi_action_set(WifiAction *action, const char *operation,
                            const char *argument);
void anto_wifi_request_free(WifiRequest *request);
void anto_wifi_action_finished(GObject *source, GAsyncResult *result,
                                 gpointer data);
void anto_wifi_run_action(MenuApp *app, const char *operation,
                            const char *argument, const char *input,
                            GtkWidget *source, GtkWidget *status,
                            gboolean return_to_wifi);
void anto_wifi_action_clicked(GtkButton *button, gpointer data);
GtkWidget *anto_wifi_action_button(MenuApp *app, const char *label,
                                     const char *icon, const char *operation,
                                     const char *argument, const char *style);
void anto_wifi_open_editor(GtkButton *button, gpointer data);
GtkWidget *anto_wifi_editor_button(MenuApp *app);
void anto_wifi_radio_changed(GObject *object, GParamSpec *spec,
                               gpointer data);
GtkWidget *anto_wifi_metric(const char *name, GtkWidget **value_out);
GtkWidget *anto_wifi_hero(MenuApp *app, WifiView *view);
void anto_wifi_password_free(gpointer data, GClosure *closure);
void anto_wifi_password_submit(GtkButton *button, gpointer data);
void anto_wifi_password_cancel(GtkButton *button, gpointer data);
void anto_wifi_show_password(MenuApp *app, const char *ssid);
void anto_wifi_connect_clicked(GtkButton *button, gpointer data);
GtkWidget *anto_wifi_connect_button(MenuApp *app,
                                      const WifiNetwork *network);
void anto_wifi_group_free(gpointer data);
void anto_wifi_card_ref_free(gpointer data);
void anto_wifi_view_free(gpointer data);
WifiGroup *anto_wifi_group_append(WifiView *view, GtkWidget *shell,
                                    const char *title);
void anto_wifi_card_update(WifiCardRef *reference,
                             const WifiNetwork *network);
WifiCardRef *anto_wifi_network_card(MenuApp *app,
                                      const WifiNetwork *network);
void anto_wifi_group_add_card(WifiView *view, guint group_index,
                                WifiCardRef *reference);
GtkWidget *anto_wifi_empty(const char *icon_name, const char *title,
                             const char *subtitle, gboolean spinner);
WifiView *anto_wifi_current_view(MenuApp *app);
guint anto_wifi_network_group(const WifiNetwork *network);
WifiCardRef *anto_wifi_card_find(WifiView *view, const char *key);
void anto_wifi_card_move(WifiView *view, WifiCardRef *reference,
                           guint group_index);
void anto_wifi_cards_reconcile(WifiView *view,
                                 const WifiSnapshot *snapshot);
void anto_wifi_hero_update(WifiView *view,
                             const WifiSnapshot *snapshot);
void anto_wifi_view_apply_snapshot(WifiView *view,
                                     const WifiSnapshot *snapshot);
void anto_wifi_search(MenuApp *app, const char *query, gpointer data);
gboolean anto_wifi_restore_scroll(gpointer data);
void anto_wifi_schedule_scroll_restore(WifiView *view, double value);
void anto_wifi_render(MenuApp *app, const WifiSnapshot *snapshot);
void anto_wifi_render_loading(MenuApp *app);
void anto_wifi_start_snapshot(MenuApp *app);
gboolean anto_wifi_debounced_refresh(gpointer data);
void menu_wifi_live_event(MenuApp *app);
void menu_show_wifi(MenuApp *app);
