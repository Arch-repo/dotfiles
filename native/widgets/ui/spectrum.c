#include "widgets.h"
#include <math.h>
typedef struct {
  WidgetStore *store;
  GtkWidget *canvas, *caption;
  double shown[WIDGET_BARS];
  gint64 time;
  guint tick;
} View;
static void draw(GtkDrawingArea *area, cairo_t *cr, int width, int height,
                 gpointer data) {
  View *v = data;
  GdkRGBA color;
  gtk_widget_get_color(GTK_WIDGET(area), &color);
  gdk_cairo_set_source_rgba(cr, &color);
  double slot = (double)width / WIDGET_BARS,
         bar = MAX(2, slot - ANTO_SPACING_XS);
  for (int i = 0; i < WIDGET_BARS; i++) {
    double h = MAX(2, v->shown[i] * height), x = i * slot + (slot - bar) / 2,
           r = MIN(bar / 2, h / 2);
    cairo_new_sub_path(cr);
    cairo_arc(cr, x + bar - r, height - h + r, r, -G_PI_2, 0);
    cairo_arc(cr, x + bar - r, height - r, r, 0, G_PI_2);
    cairo_arc(cr, x + r, height - r, r, G_PI_2, G_PI);
    cairo_arc(cr, x + r, height - h + r, r, G_PI, 3 * G_PI_2);
    cairo_close_path(cr);
    cairo_fill(cr);
  }
}
static gboolean tick(GtkWidget *canvas, GdkFrameClock *clock, gpointer data) {
  View *v = data;
  gint64 now = gdk_frame_clock_get_frame_time(clock);
  double dt = v->time ? MIN((now - v->time) / 1000000.0, .1) : .016;
  v->time = now;
  gboolean changing = FALSE;
  for (int i = 0; i < WIDGET_BARS; i++) {
    double target = v->store->bars[i], difference = target - v->shown[i];
    if (fabs(difference) > .001) {
      v->shown[i] +=
          difference * (1 - exp(-dt / (difference > 0 ? .055 : .18)));
      changing = TRUE;
    } else
      v->shown[i] = target;
  }
  if (changing)
    gtk_widget_queue_draw(canvas);
  if (!changing) {
    v->tick = 0;
    v->time = 0;
    return G_SOURCE_REMOVE;
  }
  return G_SOURCE_CONTINUE;
}
static void update(WidgetStore *s, guint fields, GtkWidget *root) {
  View *v = g_object_get_data(G_OBJECT(root), "view");
  if (fields & (W_CHANGED_MEDIA | W_CHANGED_SPECTRUM)) {
    widget_label(v->caption, !s->audio_available ? "Audio non disponibile"
                             : s->remote_media
                                 ? "Player remoto · spettro dell’audio del PC"
                                 : "In ascolto dell’uscita audio");
    gtk_widget_set_visible(v->caption, !s->audio_available || !s->playing ||
                                           s->remote_media);
  }
  if ((fields & W_CHANGED_SPECTRUM) && !v->tick)
    v->tick = gtk_widget_add_tick_callback(v->canvas, tick, v, NULL);
}
GtkWidget *widget_spectrum_new(WidgetStore *s) {
  View *v = g_new0(View, 1);
  v->store = s;
  GtkWidget *root = anto_ui_stack(GTK_ORIENTATION_VERTICAL, ANTO_SPACING_MD,
                                  "widget-spectrum");
  v->canvas = gtk_drawing_area_new();
  gtk_widget_add_css_class(v->canvas, "widget-spectrum-bars");
  gtk_widget_set_vexpand(v->canvas, TRUE);
  gtk_drawing_area_set_content_height(GTK_DRAWING_AREA(v->canvas), 80);
  gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(v->canvas), draw, v, NULL);
  gtk_box_append(GTK_BOX(root), v->canvas);
  v->caption =
      anto_ui_text("In ascolto dell’audio di sistema", "ui-caption", 1);
  gtk_box_append(GTK_BOX(root), v->caption);
  g_object_set_data_full(G_OBJECT(root), "view", v, g_free);
  g_signal_connect_object(s, "changed", G_CALLBACK(update), root, 0);
  update(s, W_CHANGED_ALL, root);
  return root;
}
