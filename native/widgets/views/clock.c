#include "widgets.h"
typedef struct {
  GtkWidget *time, *date, *weekday;
} View;
static void update(WidgetStore *store, guint fields, GtkWidget *root) {
  (void)store;
  if (!(fields & W_CHANGED_CLOCK))
    return;
  View *v = g_object_get_data(G_OBJECT(root), "view");
  g_autoptr(GDateTime) now = g_date_time_new_now_local();
  g_autofree char *time = g_date_time_format(now, "%H:%M");
  g_autofree char *date =
      g_strdup_printf("%d %s %d", g_date_time_get_day_of_month(now),
                      anto_calendar_italian_months[g_date_time_get_month(now)],
                      g_date_time_get_year(now));
  widget_label(v->time, time);
  widget_label(v->date, date);
  widget_label(v->weekday,
               anto_calendar_italian_days[g_date_time_get_day_of_week(now)]);
}
GtkWidget *widget_clock_new(WidgetStore *s) {
  View *v = g_new0(View, 1);
  GtkWidget *root =
      anto_ui_stack(GTK_ORIENTATION_VERTICAL, ANTO_SPACING_XS, "widget-clock");
  v->weekday = anto_ui_text("", "widget-accent", 1);
  v->time = anto_ui_label("", "widget-time");
  v->date = anto_ui_text("", "ui-caption", 1);
  gtk_box_append(GTK_BOX(root), v->weekday);
  gtk_box_append(GTK_BOX(root), v->time);
  gtk_box_append(GTK_BOX(root), v->date);
  g_object_set_data_full(G_OBJECT(root), "view", v, g_free);
  g_signal_connect_object(s, "changed", G_CALLBACK(update), root, 0);
  update(s, W_CHANGED_CLOCK, root);
  return root;
}

WIDGET_DECLARE(clock, "Orologio", "preferences-system-time-symbolic",
               "Ora e data", "calendar", "", 320, 200, FALSE, FALSE, 80,
               W_CHANGED_CLOCK, widget_clock_new);
