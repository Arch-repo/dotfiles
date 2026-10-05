#include "internal.h"
#include "primitives.h"

GtkWidget *anto_calendar_build_month_card(CalendarView *view) {
    GtkWidget *card = anto_ui_card(GTK_ORIENTATION_VERTICAL);
    GtkWidget *top = anto_ui_stack(GTK_ORIENTATION_HORIZONTAL, ANTO_SPACING_SM, NULL);
    view->selected_date = anto_ui_text("", "ui-summary-title", 1);
    gtk_widget_set_hexpand(view->selected_date, TRUE);
    gtk_box_append(GTK_BOX(top), view->selected_date);
    GtkWidget *today = anto_ui_action("Oggi", "office-calendar-symbolic", NULL);
    g_signal_connect(today, "clicked", G_CALLBACK(anto_calendar_today_clicked), view);
    gtk_box_append(GTK_BOX(top), today);
    gtk_box_append(GTK_BOX(card), top);
    view->calendar = GTK_CALENDAR(anto_ui_calendar());
    gtk_widget_add_css_class(GTK_WIDGET(view->calendar), "month-calendar");
    gtk_calendar_set_show_week_numbers(view->calendar, FALSE);
    if (*anto_calendar_selected_calendar_date) {
        int year = 0, month = 0, day = 0;
        if (sscanf(anto_calendar_selected_calendar_date, "%4d-%2d-%2d", &year, &month, &day) == 3) {
            g_autoptr(GDateTime) date = g_date_time_new_local(year, month, day, 12, 0, 0);
            if (date) gtk_calendar_set_date(view->calendar, date);
        }
    }
    GtkWidget *body = anto_ui_stack(GTK_ORIENTATION_HORIZONTAL, ANTO_SPACING_LG, "calendar-month-body");
    gtk_box_append(GTK_BOX(body), GTK_WIDGET(view->calendar));
    GtkWidget *day = anto_ui_stack(GTK_ORIENTATION_VERTICAL, ANTO_SPACING_SM, "calendar-day");
    gtk_widget_set_hexpand(day, TRUE);
    GtkWidget *day_header = anto_ui_stack(GTK_ORIENTATION_HORIZONTAL, ANTO_SPACING_SM, NULL);
    GtkWidget *day_title = anto_ui_text("Impegni del giorno", "item-title", 1);
    gtk_widget_set_hexpand(day_title, TRUE);
    gtk_box_append(GTK_BOX(day_header), day_title);
    view->selected_count = anto_ui_badge("0");
    gtk_box_append(GTK_BOX(day_header), view->selected_count);
    gtk_box_append(GTK_BOX(day), day_header);
    view->day_rows = g_ptr_array_new_with_free_func(anto_calendar_agenda_row_free);
    view->selected_events = anto_ui_stack(GTK_ORIENTATION_VERTICAL, ANTO_SPACING_SM, "calendar-day-events");
    view->day_empty = anto_ui_empty("office-calendar-symbolic", "Giornata libera",
                                   "Nessun impegno nella data selezionata", FALSE, NULL, NULL, NULL);
    gtk_box_append(GTK_BOX(view->selected_events), view->day_empty);
    GtkWidget *scroll = anto_ui_scroller(view->selected_events);
    gtk_widget_set_size_request(scroll, -1, 230);
    gtk_box_append(GTK_BOX(day), scroll);
    gtk_box_append(GTK_BOX(body), day);
    gtk_box_append(GTK_BOX(card), body);
    g_signal_connect(view->calendar, "notify::date", G_CALLBACK(anto_calendar_date_changed), view);
    anto_calendar_update_marks(view);
    anto_calendar_update_selection(view);
    g_object_set_data_full(G_OBJECT(card), "calendar-view", view, anto_calendar_view_free);
    return card;
}

void menu_show_calendar(MenuApp *app) {
    g_autoptr(GDateTime) now = g_date_time_new_now_local();
    g_autofree char *today = g_date_time_format(now, "%Y-%m-%d");
    g_autofree char *heading = anto_calendar_italian_date(now, TRUE);

    menu_page_begin(app, "office-calendar-symbolic", "Calendario", heading,
                    "Cerca data, evento o descrizione…");

    CalendarView *view = g_new0(CalendarView, 1);
    view->app = app;
    g_weak_ref_init(&view->window, G_OBJECT(app->window));
    view->events = anto_calendar_read_events();
    view->agenda_rows =
        g_ptr_array_new_with_free_func(anto_calendar_agenda_row_free);
    g_object_set_data(G_OBJECT(app->window), CALENDAR_VIEW_KEY, view);
    menu_add_section(app, "MESE");
    menu_append_widget(app, anto_calendar_build_month_card(view));

    menu_add_section(app, "AZIONI");
    menu_add_shell_item(app, "view-refresh-symbolic", "Sincronizza calendario",
                        "Aggiorna il feed Google configurato", NULL,
                        "$HOME/.config/anto426/remote_sync.sh calendar", FALSE);
    menu_add_nav(app, "appointment-new-symbolic", "Nuovo evento locale",
                 "Crea sul PC e sincronizza le sorgenti senza perdere i dati locali",
                 "LOCAL", "calendar-add");
    menu_add_shell_item(app, "web-browser-symbolic", "Apri calendario web",
                        "Vista completa, ricerca e gestione calendari", NULL,
                        "xdg-open 'https://calendar.google.com/calendar/u/0/r'", TRUE);

    menu_add_section(app, "AGENDA FUTURA");
    GtkWidget *agenda_header = anto_calendar_last_list_row(app);
    view->agenda_start_index = agenda_header
        ? gtk_list_box_row_get_index(GTK_LIST_BOX_ROW(agenda_header)) + 1
        : 0;
    anto_calendar_agenda_reconcile(view, today);
    menu_set_footer(app,
                    "I giorni sottolineati hanno eventi  ·  fino a 80 appuntamenti ricercabili");
}
