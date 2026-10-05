#include "widgets.h"
typedef struct {
  GtkWidget *machine, *cpu, *memory, *disk, *memory_note, *battery, *bars[3];
} View;
static void update(WidgetStore *s, guint fields, GtkWidget *root) {
  if (!(fields & W_CHANGED_HARDWARE))
    return;
  View *v = g_object_get_data(G_OBJECT(root), "view");
  widget_label(v->machine, s->machine);
  GtkWidget *values[] = {v->cpu, v->memory, v->disk};
  double fractions[] = {s->cpu, s->memory, s->disk};
  for (int i = 0; i < 3; i++) {
    g_autofree char *value = g_strdup_printf("%.0f%%", fractions[i] * 100);
    widget_label(values[i], value);
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(v->bars[i]), fractions[i]);
  }
  widget_label(v->memory_note, s->memory_text);
  widget_label(v->battery, s->battery_text);
}
GtkWidget *widget_system_new(WidgetStore *s) {
  View *v = g_new0(View, 1);
  GtkWidget *root =
      anto_ui_stack(GTK_ORIENTATION_VERTICAL, ANTO_SPACING_MD, "widget-system");
  v->machine = anto_ui_text(s->machine, "widget-machine", 1);
  gtk_box_append(GTK_BOX(root), v->machine);
  GtkWidget *metrics = anto_ui_stack(GTK_ORIENTATION_HORIZONTAL,
                                     ANTO_SPACING_LG, "widget-metrics");
  gtk_box_set_homogeneous(GTK_BOX(metrics), TRUE);
  const char *names[] = {"CPU", "Memoria", "Disco"};
  GtkWidget **values[] = {&v->cpu, &v->memory, &v->disk};
  for (int i = 0; i < 3; i++) {
    GtkWidget *metric = anto_ui_metric(names[i], values[i]);
    v->bars[i] = anto_ui_progress();
    gtk_box_append(GTK_BOX(metric), v->bars[i]);
    gtk_box_append(GTK_BOX(metrics), metric);
  }
  gtk_box_append(GTK_BOX(root), metrics);
  GtkWidget *notes =
      anto_ui_stack(GTK_ORIENTATION_HORIZONTAL, ANTO_SPACING_MD, NULL);
  v->memory_note = anto_ui_text("", "ui-caption", 1);
  gtk_widget_set_hexpand(v->memory_note, TRUE);
  v->battery = anto_ui_text("", "ui-caption", 1);
  gtk_box_append(GTK_BOX(notes), v->memory_note);
  gtk_box_append(GTK_BOX(notes), v->battery);
  gtk_box_append(GTK_BOX(root), notes);
  g_object_set_data_full(G_OBJECT(root), "view", v, g_free);
  g_signal_connect_object(s, "changed", G_CALLBACK(update), root, 0);
  update(s, W_CHANGED_HARDWARE, root);
  return root;
}

WIDGET_DECLARE(system, "Il tuo computer", "computer-symbolic",
               "CPU, memoria, disco e batteria", "hardware", "scheda_macchina",
               440, 260, TRUE, FALSE, 80, W_CHANGED_HARDWARE,
               widget_system_new);
