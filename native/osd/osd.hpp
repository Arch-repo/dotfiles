#pragma once
#include <gtk/gtk.h>
struct Osd {
  GtkApplication *application = nullptr;
  GtkWindow *window = nullptr;
  GtkWidget *icon = nullptr, *title = nullptr, *value = nullptr, *bar = nullptr;
  guint timer = 0;
};

void anto_osd_create(Osd *osd);
gboolean anto_osd_hide(gpointer data);
int anto_osd_command(GApplication *, GApplicationCommandLine *, gpointer);
