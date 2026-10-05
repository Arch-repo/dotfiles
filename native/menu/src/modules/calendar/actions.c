#include "internal.h"

void anto_calendar_date_changed(GObject *object, GParamSpec *property,
                                  gpointer data) {
    (void)object;
    (void)property;
    CalendarView *view = data;
    anto_calendar_update_marks(view);
    anto_calendar_update_selection(view);
}

void anto_calendar_today_clicked(GtkButton *button, gpointer data) {
    (void)button;
    CalendarView *view = data;
    g_autoptr(GDateTime) today = g_date_time_new_now_local();
    gtk_calendar_set_date(view->calendar, today);
}

char *anto_calendar_event_key(const CalendarEvent *event) {
    if (event->id && *event->id)
        return g_strdup_printf("%s\x1f%s", event->source, event->id);
    return g_strdup_printf("%s\x1f%s\x1f%s\x1f%s",
                           event->source, event->date, event->start,
                           event->title);
}
