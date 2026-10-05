#pragma once
/* Page-private state and cross-file contract. Public API stays in menu.h. */
#include "menu.h"

#include <json-c/json.h>
#include <string.h>
#include <time.h>

#define CALENDAR_VIEW_KEY "anto-menu-calendar-view"

#include "calendar_data.h"


typedef struct {
    char *key;
    GtkWidget *row;
    GtkWidget *title;
    GtkWidget *subtitle;
    GtkWidget *badge;
} CalendarAgendaRow;

typedef struct {
    MenuApp *app;
    GWeakRef window;
    GPtrArray *events;
    GPtrArray *agenda_rows;
    GPtrArray *day_rows;
    GtkCalendar *calendar;
    GtkWidget *selected_date;
    GtkWidget *selected_events;
    GtkWidget *selected_count;
    GtkWidget *day_empty;
    int agenda_start_index;
    char today[11];
} CalendarView;

typedef struct {
    GWeakRef window;
    MenuApp *app;
    GtkAdjustment *adjustment;
    double value;
} CalendarScrollRestore;

/* Keep the selected day when the user leaves and reopens the page. */

extern char anto_calendar_selected_calendar_date[11];
extern const char *const anto_calendar_italian_days[8];
extern const char *const anto_calendar_italian_months[13];

char *anto_calendar_italian_date(GDateTime *date, gboolean include_time);
const char *anto_calendar_json_string(struct json_object *object, const char *key);
void anto_calendar_event_free(gpointer data);
void anto_calendar_agenda_row_free(gpointer data);
void anto_calendar_view_free(gpointer data);
int anto_calendar_compare_events(gconstpointer left, gconstpointer right);
void anto_calendar_load_events_file(GPtrArray *events, GHashTable *seen,
                             const char *path);
GPtrArray *anto_calendar_read_events(void);
gboolean anto_calendar_parse_event_date(const CalendarEvent *event, int *year,
                                 int *month, int *day);
void anto_calendar_update_marks(CalendarView *view);
void anto_calendar_update_selection(CalendarView *view);
void anto_calendar_day_reconcile(CalendarView *view, const char *date);
void anto_calendar_date_changed(GObject *object, GParamSpec *property,
                                  gpointer data);
void anto_calendar_today_clicked(GtkButton *button, gpointer data);
GtkWidget *anto_calendar_build_month_card(CalendarView *view);
GtkWidget *anto_calendar_last_list_row(MenuApp *app);
GtkWidget *anto_calendar_find_class(GtkWidget *root,
                                      const char *css_class);
char *anto_calendar_event_key(const CalendarEvent *event);
char *anto_calendar_event_subtitle(const CalendarEvent *event);
const char *anto_calendar_event_badge(const CalendarEvent *event,
                                        const char *today);
CalendarAgendaRow *anto_calendar_agenda_row_add(
    CalendarView *view, const char *key, const char *title,
    const char *subtitle, const char *badge);
void anto_calendar_agenda_row_update(CalendarAgendaRow *row,
                                       const char *title,
                                       const char *subtitle,
                                       const char *badge);
CalendarAgendaRow *anto_calendar_agenda_find(CalendarView *view,
                                                const char *key);
void anto_calendar_agenda_order(CalendarView *view,
                                  GPtrArray *desired);
void anto_calendar_agenda_reconcile(CalendarView *view,
                                      const char *today);
gboolean anto_calendar_restore_scroll(gpointer data);
void anto_calendar_preserve_scroll(CalendarView *view, double value);
void menu_calendar_live_event(MenuApp *app);
void menu_calendar_clock_tick(MenuApp *app);
void menu_show_calendar(MenuApp *app);
