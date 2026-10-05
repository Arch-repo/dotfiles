#pragma once
#include <gtk/gtk.h>
G_BEGIN_DECLS
void anto_glass_bind(GtkWindow *window, GtkWidget *panel, float radius);
void anto_load_glass_style(GdkDisplay *display);
G_END_DECLS
