#include "internal.h"
#include "primitives.h"

void anto_calendar_day_reconcile(CalendarView *view, const char *date) {
    g_autoptr(GHashTable) wanted = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
    guint count = 0;
    GtkWidget *previous = view->day_empty;
    for (guint i = 0; i < view->events->len; i++) {
        CalendarEvent *event = g_ptr_array_index(view->events, i);
        if (g_strcmp0(event->date, date) != 0) continue;
        count++;
        g_autofree char *key = anto_calendar_event_key(event);
        g_hash_table_add(wanted, g_strdup(key));
        CalendarAgendaRow *row = NULL;
        for (guint j = 0; j < view->day_rows->len; j++) {
            CalendarAgendaRow *candidate = g_ptr_array_index(view->day_rows, j);
            if (g_strcmp0(candidate->key, key) == 0) { row = candidate; break; }
        }
        g_autofree char *time = event->all_day ? g_strdup("Tutto il giorno") :
            (*event->start ? g_strdup_printf("%s%s%s", event->start,
                event->end && *event->end ? "–" : "", event->end ? event->end : "") : g_strdup("Senza orario"));
        const char *source = g_strcmp0(event->source, "google") == 0 ? "Google" : "Locale";
        g_autofree char *detail = g_strdup_printf("%s · %s%s%s", time, source,
            event->description && *event->description ? "\n" : "",
            event->description ? event->description : "");
        if (!row) {
            row = g_new0(CalendarAgendaRow, 1);
            row->key = g_strdup(key);
            row->row = anto_ui_row(NULL, event->title, detail, &row->title, &row->subtitle);
            gtk_label_set_wrap(GTK_LABEL(row->title), TRUE);
            gtk_label_set_lines(GTK_LABEL(row->title), 2);
            gtk_label_set_lines(GTK_LABEL(row->subtitle), 3);
            gtk_box_append(GTK_BOX(view->selected_events), row->row);
            g_ptr_array_add(view->day_rows, row);
        } else anto_calendar_agenda_row_update(row, event->title, detail, "");
        gtk_widget_set_tooltip_text(row->row, detail);
        gtk_box_reorder_child_after(GTK_BOX(view->selected_events), row->row, previous);
        previous = row->row;
    }
    for (guint i = view->day_rows->len; i > 0; i--) {
        CalendarAgendaRow *row = g_ptr_array_index(view->day_rows, i - 1);
        if (g_hash_table_contains(wanted, row->key)) continue;
        gtk_box_remove(GTK_BOX(view->selected_events), row->row);
        g_ptr_array_remove_index(view->day_rows, i - 1);
    }
    g_autofree char *total = g_strdup_printf("%u", count);
    gtk_label_set_text(GTK_LABEL(view->selected_count), total);
    gtk_widget_set_visible(view->day_empty, count == 0);
}

GtkWidget *anto_calendar_last_list_row(MenuApp *app) {
    int index = 0;
    GtkListBoxRow *row = NULL;
    while ((row = gtk_list_box_get_row_at_index(
                GTK_LIST_BOX(app->list), index)) != NULL)
        index++;
    return index > 0 ? GTK_WIDGET(gtk_list_box_get_row_at_index(
                           GTK_LIST_BOX(app->list), index - 1))
                     : NULL;
}

CalendarAgendaRow *anto_calendar_agenda_row_add(
    CalendarView *view, const char *key, const char *title,
    const char *subtitle, const char *badge) {
    menu_add_item(view->app, "office-calendar-symbolic", title, subtitle,
                  badge, NULL, NULL, NULL);
    GtkWidget *widget = anto_calendar_last_list_row(view->app);
    CalendarAgendaRow *row = g_new0(CalendarAgendaRow, 1);
    row->key = g_strdup(key);
    row->row = widget;
    row->title = anto_calendar_find_class(widget, "item-title");
    row->subtitle = anto_calendar_find_class(widget, "item-subtitle");
    row->badge = anto_calendar_find_class(widget, "item-badge");
    g_ptr_array_add(view->agenda_rows, row);
    return row;
}

void anto_calendar_agenda_row_update(CalendarAgendaRow *row,
                                       const char *title,
                                       const char *subtitle,
                                       const char *badge) {
    if (GTK_IS_LABEL(row->title) &&
        g_strcmp0(gtk_label_get_text(GTK_LABEL(row->title)), title) != 0)
        gtk_label_set_text(GTK_LABEL(row->title), title);
    if (GTK_IS_LABEL(row->subtitle) &&
        g_strcmp0(gtk_label_get_text(GTK_LABEL(row->subtitle)), subtitle) != 0)
        gtk_label_set_text(GTK_LABEL(row->subtitle), subtitle);
    if (GTK_IS_LABEL(row->badge) &&
        g_strcmp0(gtk_label_get_text(GTK_LABEL(row->badge)), badge) != 0)
        gtk_label_set_text(GTK_LABEL(row->badge), badge);
    g_autofree char *combined =
        g_strdup_printf("%s %s %s", title, subtitle, badge ? badge : "");
    g_autofree char *search = g_utf8_strdown(combined, -1);
    g_object_set_data_full(G_OBJECT(row->row), "menu-search",
                           g_strdup(search), g_free);
}

void anto_calendar_agenda_reconcile(CalendarView *view,
                                      const char *today) {
    g_autoptr(GHashTable) wanted =
        g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
    g_autoptr(GPtrArray) desired = g_ptr_array_new();
    guint added = 0;
    for (guint i = 0; i < view->events->len && added < 80; i++) {
        CalendarEvent *event = g_ptr_array_index(view->events, i);
        if (g_strcmp0(event->date, today) < 0) continue;
        g_autofree char *key = anto_calendar_event_key(event);
        g_hash_table_add(wanted, g_strdup(key));
        g_autofree char *subtitle = anto_calendar_event_subtitle(event);
        const char *badge = anto_calendar_event_badge(event, today);
        CalendarAgendaRow *row = anto_calendar_agenda_find(view, key);
        if (!row)
            row = anto_calendar_agenda_row_add(
                view, key, event->title, subtitle, badge);
        else
            anto_calendar_agenda_row_update(
                row, event->title, subtitle, badge);
        g_ptr_array_add(desired, row);
        added++;
    }

    if (!added) {
        const char *key = "fixed:empty";
        g_hash_table_add(wanted, g_strdup(key));
        CalendarAgendaRow *row = anto_calendar_agenda_find(view, key);
        if (!row)
            row = anto_calendar_agenda_row_add(
                view, key, "Agenda libera",
                "Nessun evento futuro sincronizzato", "");
        else
            anto_calendar_agenda_row_update(
                row, "Agenda libera",
                "Nessun evento futuro sincronizzato", "");
        g_ptr_array_add(desired, row);
    }

    for (guint i = view->agenda_rows->len; i > 0; i--) {
        CalendarAgendaRow *row =
            g_ptr_array_index(view->agenda_rows, i - 1);
        if (g_hash_table_contains(wanted, row->key)) continue;
        if (row->row && gtk_widget_get_parent(row->row) == view->app->list)
            gtk_list_box_remove(GTK_LIST_BOX(view->app->list), row->row);
        g_ptr_array_remove_index(view->agenda_rows, i - 1);
    }
    anto_calendar_agenda_order(view, desired);
    g_strlcpy(view->today, today, sizeof(view->today));
}

gboolean anto_calendar_restore_scroll(gpointer data) {
    CalendarScrollRestore *restore = data;
    GObject *window = g_weak_ref_get(&restore->window);
    if (window &&
        g_strcmp0(restore->app->current_page, "calendar") == 0) {
        double lower = gtk_adjustment_get_lower(restore->adjustment);
        double maximum =
            MAX(lower, gtk_adjustment_get_upper(restore->adjustment) -
                           gtk_adjustment_get_page_size(restore->adjustment));
        gtk_adjustment_set_value(
            restore->adjustment,
            CLAMP(restore->value, lower, maximum));
    }
    g_clear_object(&window);
    g_clear_object(&restore->adjustment);
    g_weak_ref_clear(&restore->window);
    g_free(restore);
    return G_SOURCE_REMOVE;
}

void anto_calendar_preserve_scroll(CalendarView *view, double value) {
    CalendarScrollRestore *restore =
        g_new0(CalendarScrollRestore, 1);
    restore->app = view->app;
    restore->value = value;
    restore->adjustment = g_object_ref(
        gtk_scrolled_window_get_vadjustment(
            GTK_SCROLLED_WINDOW(view->app->list_scroll)));
    g_weak_ref_init(&restore->window, G_OBJECT(view->app->window));
    g_idle_add(anto_calendar_restore_scroll, restore);
}
