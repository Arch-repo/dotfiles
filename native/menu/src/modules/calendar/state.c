#include "internal.h"

char anto_calendar_selected_calendar_date[11];
void anto_calendar_agenda_row_free(gpointer data) {
    CalendarAgendaRow *row = data;
    if (!row) return;
    g_free(row->key);
    g_free(row);
}

void anto_calendar_view_free(gpointer data) {
    CalendarView *view = data;
    if (!view) return;
    GObject *window = g_weak_ref_get(&view->window);
    if (window &&
        g_object_get_data(window, CALENDAR_VIEW_KEY) == view)
        g_object_set_data(window, CALENDAR_VIEW_KEY, NULL);
    g_clear_object(&window);
    g_weak_ref_clear(&view->window);
    g_clear_pointer(&view->events, g_ptr_array_unref);
    g_clear_pointer(&view->agenda_rows, g_ptr_array_unref);
    g_clear_pointer(&view->day_rows, g_ptr_array_unref);
    g_free(view);
}

gboolean anto_calendar_parse_event_date(const CalendarEvent *event, int *year,
                                 int *month, int *day) {
    return event && sscanf(event->date, "%4d-%2d-%2d", year, month, day) == 3 &&
           *year > 0 && *month >= 1 && *month <= 12 && *day >= 1 && *day <= 31;
}

void anto_calendar_update_marks(CalendarView *view) {
    gtk_calendar_clear_marks(view->calendar);
    g_autoptr(GDateTime) shown = gtk_calendar_get_date(view->calendar);
    if (!shown) return;
    int shown_year = g_date_time_get_year(shown);
    int shown_month = g_date_time_get_month(shown);
    for (guint i = 0; i < view->events->len; i++) {
        CalendarEvent *event = g_ptr_array_index(view->events, i);
        int year = 0, month = 0, day = 0;
        if (anto_calendar_parse_event_date(event, &year, &month, &day) &&
            year == shown_year && month == shown_month)
            gtk_calendar_mark_day(view->calendar, (guint)day);
    }
}

void anto_calendar_update_selection(CalendarView *view) {
    g_autoptr(GDateTime) selected = gtk_calendar_get_date(view->calendar);
    if (!selected) return;
    g_autofree char *iso = g_date_time_format(selected, "%Y-%m-%d");
    g_strlcpy(anto_calendar_selected_calendar_date, iso, sizeof(anto_calendar_selected_calendar_date));
    g_autofree char *pretty = anto_calendar_italian_date(selected, FALSE);
    gtk_label_set_text(GTK_LABEL(view->selected_date), pretty);

    anto_calendar_day_reconcile(view, iso);
}

GtkWidget *anto_calendar_find_class(GtkWidget *root,
                                      const char *css_class) {
    if (!root) return NULL;
    if (gtk_widget_has_css_class(root, css_class)) return root;
    for (GtkWidget *child = gtk_widget_get_first_child(root);
         child; child = gtk_widget_get_next_sibling(child)) {
        GtkWidget *match = anto_calendar_find_class(child, css_class);
        if (match) return match;
    }
    return NULL;
}

char *anto_calendar_event_subtitle(const CalendarEvent *event) {
    const char *source = g_strcmp0(event->source, "google") == 0
                             ? "Google"
                             : "Locale";
    const char *when = event->all_day ? "tutto il giorno" : event->start;
    return g_strdup_printf(
        "%s · %s%s%s%s%s", source, event->date, *when ? " · " : "",
        when, *event->description ? " · " : "", event->description);
}

const char *anto_calendar_event_badge(const CalendarEvent *event,
                                        const char *today) {
    if (g_strcmp0(event->date, today) == 0) return "OGGI";
    return g_strcmp0(event->source, "google") == 0 ? "GOOGLE" : "LOCALE";
}

CalendarAgendaRow *anto_calendar_agenda_find(CalendarView *view,
                                                const char *key) {
    for (guint i = 0; i < view->agenda_rows->len; i++) {
        CalendarAgendaRow *row = g_ptr_array_index(view->agenda_rows, i);
        if (g_strcmp0(row->key, key) == 0) return row;
    }
    return NULL;
}

void anto_calendar_agenda_order(CalendarView *view,
                                  GPtrArray *desired) {
    for (guint i = 0; i < desired->len; i++) {
        CalendarAgendaRow *row = g_ptr_array_index(desired, i);
        int target = view->agenda_start_index + (int)i;
        int current = gtk_list_box_row_get_index(
            GTK_LIST_BOX_ROW(row->row));
        if (current != target) {
            g_object_ref(row->row);
            gtk_list_box_remove(GTK_LIST_BOX(view->app->list), row->row);
            gtk_list_box_insert(GTK_LIST_BOX(view->app->list),
                                row->row, target);
            g_object_unref(row->row);
        }
        for (guint j = i; j < view->agenda_rows->len; j++) {
            if (g_ptr_array_index(view->agenda_rows, j) != row) continue;
            if (j != i) {
                gpointer moved =
                    g_ptr_array_steal_index(view->agenda_rows, j);
                g_ptr_array_insert(view->agenda_rows, i, moved);
            }
            break;
        }
    }
}

void menu_calendar_live_event(MenuApp *app) {
    if (!app || !app->window ||
        g_strcmp0(app->current_page, "calendar") != 0)
        return;
    CalendarView *view =
        g_object_get_data(G_OBJECT(app->window), CALENDAR_VIEW_KEY);
    if (!view || view->app != app) return;
    GtkAdjustment *adjustment =
        gtk_scrolled_window_get_vadjustment(
            GTK_SCROLLED_WINDOW(app->list_scroll));
    double scroll = gtk_adjustment_get_value(adjustment);

    g_clear_pointer(&view->events, g_ptr_array_unref);
    view->events = anto_calendar_read_events();
    anto_calendar_update_marks(view);
    anto_calendar_update_selection(view);

    g_autoptr(GDateTime) now = g_date_time_new_now_local();
    g_autofree char *today = g_date_time_format(now, "%Y-%m-%d");
    g_autofree char *heading = anto_calendar_italian_date(now, TRUE);
    gtk_label_set_text(GTK_LABEL(app->page_subtitle), heading);
    anto_calendar_agenda_reconcile(view, today);
    gtk_list_box_invalidate_filter(GTK_LIST_BOX(app->list));
    anto_calendar_preserve_scroll(view, scroll);
}

void menu_calendar_clock_tick(MenuApp *app) {
    if (!app || !app->window ||
        g_strcmp0(app->current_page, "calendar") != 0)
        return;
    CalendarView *view =
        g_object_get_data(G_OBJECT(app->window), CALENDAR_VIEW_KEY);
    if (!view || view->app != app) return;
    g_autoptr(GDateTime) now = g_date_time_new_now_local();
    g_autofree char *heading = anto_calendar_italian_date(now, TRUE);
    if (g_strcmp0(gtk_label_get_text(GTK_LABEL(app->page_subtitle)),
                  heading) != 0)
        gtk_label_set_text(GTK_LABEL(app->page_subtitle), heading);
    g_autofree char *today = g_date_time_format(now, "%Y-%m-%d");
    if (g_strcmp0(view->today, today) != 0) {
        GtkAdjustment *adjustment =
            gtk_scrolled_window_get_vadjustment(
                GTK_SCROLLED_WINDOW(app->list_scroll));
        double scroll = gtk_adjustment_get_value(adjustment);
        anto_calendar_agenda_reconcile(view, today);
        gtk_list_box_invalidate_filter(GTK_LIST_BOX(app->list));
        anto_calendar_preserve_scroll(view, scroll);
    }
}
