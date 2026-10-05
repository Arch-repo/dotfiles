#pragma once
/* Page-private state and cross-file contract. Public API stays in menu.h. */
#include "menu.h"

#include <json-c/json.h>
#include <string.h>

typedef struct {
    MenuApp *app;
    GtkCalendar *calendar;
    GtkWidget *title;
    GtkWidget *start;
    GtkWidget *end;
    GtkWidget *all_day;
    GtkTextBuffer *description;
    GtkWidget *status;
} CalendarEditor;



char *anto_calendar_editor_calendar_adapter(void);
void anto_calendar_editor_editor_status(CalendarEditor *editor, const char *message,
                          gboolean error);
gboolean anto_calendar_editor_valid_time(const char *value);
void anto_calendar_editor_return_to_calendar(CalendarEditor *editor);
void anto_calendar_editor_save_event(GtkButton *button, gpointer data);
void anto_calendar_editor_all_day_changed(GtkSwitch *toggle, GParamSpec *property,
                            gpointer data);
GtkWidget *anto_calendar_editor_build_editor(CalendarEditor *editor);
void menu_show_calendar_add(MenuApp *app);
