#include "widgets.h"
#include <string.h>
typedef struct {
  GtkWidget *date, *rows;
  char today[11];
} View;
static void update(WidgetStore *s, guint fields, GtkWidget *root) {
  if (!(fields & (W_CHANGED_AGENDA | W_CHANGED_CLOCK)))
    return;
  View *v = g_object_get_data(G_OBJECT(root), "view");
  g_autoptr(GDateTime) now = g_date_time_new_now_local();
  g_autofree char *today = g_date_time_format(now, "%Y-%m-%d");
  if (!(fields & W_CHANGED_AGENDA) && !strcmp(v->today, today))
    return;
  g_strlcpy(v->today, today, sizeof(v->today));
  g_autofree char *date = anto_calendar_italian_date(now, FALSE);
  widget_label(v->date, date);
  GtkWidget *child;
  while ((child = gtk_widget_get_first_child(v->rows)))
    gtk_box_remove(GTK_BOX(v->rows), child);
  g_autoptr(GDateTime) horizon = g_date_time_add_days(now, 30);
  g_autofree char *last = g_date_time_format(horizon, "%Y-%m-%d");
  g_autofree char *time = g_date_time_format(now, "%H:%M");
  guint count = 0;
  for (guint i = 0; i < s->events->len && count < 5; i++) {
    CalendarEvent *e = g_ptr_array_index(s->events, i);
    if (strcmp(e->date, today) < 0 || strcmp(e->date, last) > 0)
      continue;
    if (!strcmp(e->date, today) && !e->all_day && e->end && *e->end &&
        strcmp(e->end, time) < 0)
      continue;
    const char *when = e->all_day ? "Tutto il giorno" : e->start;
    g_autofree char *iso = g_strconcat(e->date, "T00:00:00", NULL);
    g_autoptr(GTimeZone) zone = g_time_zone_new_local();
    g_autoptr(GDateTime) event_date = g_date_time_new_from_iso8601(iso, zone);
    g_autofree char *day =
        event_date ? g_strdup_printf(
                         "%d %s", g_date_time_get_day_of_month(event_date),
                         anto_calendar_italian_months[g_date_time_get_month(
                             event_date)])
                   : g_strdup(e->date);
    g_autofree char *detail =
        g_strdup_printf("%s · %s", !strcmp(e->date, today) ? "Oggi" : day,
                        when && *when ? when : "Senza orario");
    GtkWidget *row = anto_ui_row(
        anto_ui_icon("x-office-calendar-symbolic", ANTO_SIZE_ICON, NULL),
        e->title, detail, NULL, NULL);
    gtk_widget_add_css_class(row, "widget-event");
    gtk_box_append(GTK_BOX(v->rows), row);
    count++;
  }
  if (!count)
    gtk_box_append(GTK_BOX(v->rows), anto_ui_empty("x-office-calendar-symbolic",
                                                   "Nessun impegno in arrivo",
                                                   "Goditi il tuo tempo", FALSE,
                                                   NULL, NULL, NULL));
}
GtkWidget *widget_agenda_new(WidgetStore *s) {
  View *v = g_new0(View, 1);
  GtkWidget *root =
      anto_ui_stack(GTK_ORIENTATION_VERTICAL, ANTO_SPACING_SM, "widget-agenda");
  v->date = anto_ui_text("", "widget-accent", 1);
  gtk_box_append(GTK_BOX(root), v->date);
  v->rows =
      anto_ui_stack(GTK_ORIENTATION_VERTICAL, ANTO_SPACING_XS, "widget-events");
  gtk_box_append(GTK_BOX(root), anto_ui_scroller(v->rows));
  g_object_set_data_full(G_OBJECT(root), "view", v, g_free);
  g_signal_connect_object(s, "changed", G_CALLBACK(update), root, 0);
  update(s, W_CHANGED_AGENDA, root);
  return root;
}

WIDGET_DECLARE(calendar, "Agenda", "x-office-calendar-symbolic",
               "Impegni locali e Google", "calendar", "calendario", 420, 240,
               FALSE, FALSE, 304, W_CHANGED_CLOCK | W_CHANGED_AGENDA,
               widget_agenda_new);
