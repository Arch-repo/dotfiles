#pragma once

#include <gtk/gtk.h>
#include "query.h"

typedef struct _MenuApp MenuApp;
typedef struct _MenuLive MenuLive;
typedef void (*MenuAction)(MenuApp *app, gpointer data);
typedef void (*MenuScaleAction)(MenuApp *app, double value, gpointer data);
typedef void (*MenuSearchAction)(MenuApp *app, const char *query, gpointer data);
typedef gboolean (*MenuKeyAction)(guint key, GdkModifierType modifiers, gpointer data);

typedef enum {
    MENU_LAYOUT_LIST,
    MENU_LAYOUT_GRID,
    MENU_LAYOUT_CUSTOM,
} MenuLayout;

struct _MenuApp {
    GtkApplication *application;
    GtkWindow *window;
    GtkWidget *overlay;
    GtkWidget *deck;
    GtkWidget *context_rail;
    GtkWidget *panel;
    GtkWidget *nav_rail;
    GtkWidget *back_button;
    GtkWidget *page_icon;
    GtkWidget *page_title;
    GtkWidget *page_subtitle;
    GtkWidget *search;
    GtkWidget *content_stack;
    GtkWidget *list_scroll;
    GtkWidget *list;
    GtkWidget *grid_scroll;
    GtkWidget *grid;
    GtkWidget *custom_holder;
    GtkWidget *context_page;
    GtkWidget *context_time;
    GtkWidget *context_date;
    GtkWidget *context_network_card;
    GtkWidget *context_network_icon;
    GtkWidget *context_network_value;
    GtkWidget *context_audio_card;
    GtkWidget *context_audio_icon;
    GtkWidget *context_audio_value;
    GtkWidget *context_display_card;
    GtkWidget *context_display_icon;
    GtkWidget *context_display_value;
    GtkWidget *context_battery_card;
    GtkWidget *context_battery_icon;
    GtkWidget *context_battery_value;
    GtkWidget *footer;
    GPtrArray *rail_buttons;
    GPtrArray *history;
    MenuLayout layout;
    guint grid_columns;
    guint clock_source;
    guint status_source;
    gboolean closing;
    AntoQuery *summary;
    char *desktop_snapshot;
    MenuLive *live;
    MenuSearchAction search_action;
    gpointer search_data;
    MenuKeyAction key_action;
    gpointer key_data;
    char *current_page;
    char *requested_page;
};

void menu_build_window(MenuApp *app);
void menu_context_status_refresh(MenuApp *app);
void menu_system_accept(MenuApp *app, const char *output);
void menu_live_start(MenuApp *app);
void menu_live_stop(MenuApp *app);
void menu_target_active_monitor(MenuApp *app);
void menu_open(MenuApp *app, const char *page);
gboolean menu_known_page(const char *page);
void menu_print_pages(void);
void menu_back(MenuApp *app);
void menu_close(MenuApp *app);

void menu_page_begin(MenuApp *app, const char *icon, const char *title,
                     const char *subtitle, const char *placeholder);
void menu_set_layout(MenuApp *app, MenuLayout layout);
void menu_set_grid_columns(MenuApp *app, guint columns);
void menu_set_custom_content(MenuApp *app, GtkWidget *widget);
void menu_set_search_action(MenuApp *app, MenuSearchAction action,
                            gpointer data, GDestroyNotify destroy);
void menu_add_section(MenuApp *app, const char *title);
void menu_add_item(MenuApp *app, const char *icon, const char *title,
                   const char *subtitle, const char *badge,
                   MenuAction action, gpointer data, GDestroyNotify destroy);
void menu_add_nav(MenuApp *app, const char *icon, const char *title,
                  const char *subtitle, const char *badge, const char *page);
void menu_add_tile(MenuApp *app, const char *icon, const char *title,
                   const char *subtitle, const char *badge,
                   MenuAction action, gpointer data, GDestroyNotify destroy);
void menu_add_nav_tile(MenuApp *app, const char *icon, const char *title,
                       const char *subtitle, const char *badge, const char *page);
void menu_add_shell_item(MenuApp *app, const char *icon, const char *title,
                         const char *subtitle, const char *badge,
                         const char *command, gboolean close_after);
void menu_add_shell_tile(MenuApp *app, const char *icon, const char *title,
                         const char *subtitle, const char *badge,
                         const char *command, gboolean close_after);
void menu_add_scale(MenuApp *app, const char *icon, const char *title,
                    const char *subtitle, double value, double min,
                    double max, double step, MenuScaleAction action,
                    gpointer data, GDestroyNotify destroy);
void menu_append_widget(MenuApp *app, GtkWidget *widget);
void menu_set_footer(MenuApp *app, const char *text);

void menu_spawn(MenuApp *app, const char *const argv[], gboolean close_after);
void menu_spawn_shell(MenuApp *app, const char *command, gboolean close_after);
char *menu_capture(const char *command);
gboolean menu_run_with_input(const char *const argv[], const char *input,
                             char **stdout_text, char **stderr_text);
void menu_notify(const char *title, const char *body);
void menu_open_terminal(MenuApp *app, const char *title, const char *command);
char *menu_config_path(const char *suffix);
char *menu_home_path(const char *suffix);

void menu_show_launcher(MenuApp *app);
void menu_launcher_live_event(MenuApp *app);
void menu_show_system(MenuApp *app);
void menu_system_live_event(MenuApp *app);
void menu_show_audio(MenuApp *app);
void menu_audio_live_event(MenuApp *app);
void menu_show_wifi(MenuApp *app);
void menu_wifi_live_event(MenuApp *app);
void menu_show_bluetooth(MenuApp *app);
void menu_bluetooth_live_event(MenuApp *app);
void menu_bluetooth_agent_start(MenuApp *app);
void menu_bluetooth_agent_dismiss(MenuApp *app);
void menu_show_brightness(MenuApp *app);
void menu_brightness_live_event(MenuApp *app);
void menu_show_display(MenuApp *app);
void menu_display_live_event(MenuApp *app);
void menu_show_capture(MenuApp *app);
void menu_show_record(MenuApp *app);
void menu_show_power(MenuApp *app);
void menu_show_clipboard(MenuApp *app);
void menu_clipboard_live_event(MenuApp *app);
void menu_show_emoji(MenuApp *app);
void menu_show_hardware(MenuApp *app);
void menu_hardware_live_event(MenuApp *app);
void menu_show_notifications(MenuApp *app);
void menu_notifications_live_event(MenuApp *app);
void menu_show_calendar(MenuApp *app);
void menu_calendar_live_event(MenuApp *app);
void menu_calendar_clock_tick(MenuApp *app);
void menu_show_calendar_add(MenuApp *app);
void menu_show_keyboard(MenuApp *app);
void menu_keyboard_live_event(MenuApp *app);
void menu_show_widgets(MenuApp *app);
void menu_show_floating(MenuApp *app);
void menu_floating_live_event(MenuApp *app);
void menu_show_wallpaper(MenuApp *app);
void menu_show_background(MenuApp *app);
void menu_background_live_event(MenuApp *app);
void menu_show_shortcuts(MenuApp *app);
void menu_show_settings(MenuApp *app);

int menu_run_direct_action(int argc, char **argv);

char *menu_backend_path(void);
void menu_spawn_backend(MenuApp *app, const char *domain,
                        const char *operation, const char *argument,
                        const char *value, gboolean close_after);
void menu_add_backend_item(MenuApp *app, const char *icon,
                           const char *title, const char *subtitle,
                           const char *badge, const char *domain,
                           const char *operation, const char *argument,
                           const char *value, gboolean close_after);
void menu_add_backend_tile(MenuApp *app, const char *icon,
                           const char *title, const char *subtitle,
                           const char *badge, const char *domain,
                           const char *operation, const char *argument,
                           const char *value, gboolean close_after);
