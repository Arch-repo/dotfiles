#include "widgets.h"
typedef struct {
  WidgetStore *store;
  GtkWidget *art, *title, *artist, *previous, *play, *next;
} View;
static void clicked(GtkButton *button, gpointer data) {
  widget_media_control(data, g_object_get_data(G_OBJECT(button), "method"));
}
static void update(WidgetStore *s, guint fields, GtkWidget *root) {
  if (!(fields & W_CHANGED_MEDIA))
    return;
  View *v = g_object_get_data(G_OBJECT(root), "view");
  widget_label(v->title, s->track);
  widget_label(v->artist, s->artist);
  gtk_picture_set_filename(GTK_PICTURE(v->art), s->art);
  gtk_widget_set_visible(v->art, s->art && *s->art);
  gtk_button_set_icon_name(GTK_BUTTON(v->play),
                           s->playing ? "media-playback-pause-symbolic"
                                      : "media-playback-start-symbolic");
  gtk_widget_set_sensitive(v->play, s->playing ? s->can_pause : s->can_play);
  gtk_widget_set_sensitive(v->previous, s->can_previous);
  gtk_widget_set_sensitive(v->next, s->can_next);
}
GtkWidget *widget_media_new(WidgetStore *s) {
  View *v = g_new0(View, 1);
  v->store = s;
  GtkWidget *root =
      anto_ui_stack(GTK_ORIENTATION_VERTICAL, ANTO_SPACING_MD, "widget-media");
  GtkWidget *track =
      anto_ui_stack(GTK_ORIENTATION_HORIZONTAL, ANTO_SPACING_MD, NULL);
  v->art = anto_ui_picture(NULL);
  gtk_widget_set_size_request(v->art, 68, 68);
  gtk_widget_set_hexpand(v->art, FALSE);
  gtk_widget_add_css_class(v->art, "widget-art");
  gtk_box_append(GTK_BOX(track), v->art);
  GtkWidget *copy = anto_ui_copy(s->track, s->artist, &v->title, &v->artist);
  gtk_box_append(GTK_BOX(track), copy);
  gtk_box_append(GTK_BOX(root), track);
  GtkWidget *controls = anto_ui_stack(GTK_ORIENTATION_HORIZONTAL,
                                      ANTO_SPACING_SM, "widget-playback");
  gtk_widget_set_halign(controls, GTK_ALIGN_CENTER);
  GtkWidget **buttons[] = {&v->previous, &v->play, &v->next};
  const char *icons[] = {"media-skip-backward-symbolic",
                         "media-playback-start-symbolic",
                         "media-skip-forward-symbolic"};
  const char *methods[] = {"Previous", "PlayPause", "Next"};
  const char *labels[] = {"Traccia precedente", "Play / pausa",
                          "Traccia successiva"};
  for (int i = 0; i < 3; i++) {
    *buttons[i] = anto_ui_icon_button(icons[i], labels[i]);
    gtk_widget_set_focusable(*buttons[i], FALSE);
    g_object_set_data(G_OBJECT(*buttons[i]), "method", (gpointer)methods[i]);
    g_signal_connect(*buttons[i], "clicked", G_CALLBACK(clicked), s);
    gtk_box_append(GTK_BOX(controls), *buttons[i]);
  }
  gtk_box_append(GTK_BOX(root), controls);
  gtk_box_append(GTK_BOX(root), widget_spectrum_new(s));
  g_object_set_data_full(G_OBJECT(root), "view", v, g_free);
  g_signal_connect_object(s, "changed", G_CALLBACK(update), root, 0);
  update(s, W_CHANGED_MEDIA, root);
  return root;
}

WIDGET_DECLARE(media, "Musica", "audio-x-generic-symbolic",
               "Player e spettro audio", "audio",
               "spotify spectrum spettro_audio cava", 440, 340, TRUE, FALSE,
               380, W_CHANGED_MEDIA | W_CHANGED_SPECTRUM, widget_media_new);
