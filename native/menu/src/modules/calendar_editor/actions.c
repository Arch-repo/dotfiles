#include "internal.h"

void anto_calendar_editor_all_day_changed(GtkSwitch *toggle, GParamSpec *property,
                            gpointer data) {
    (void)property;
    CalendarEditor *editor = data;
    gboolean timed = !gtk_switch_get_active(toggle);
    gtk_widget_set_sensitive(editor->start, timed);
    gtk_widget_set_sensitive(editor->end, timed);
}
