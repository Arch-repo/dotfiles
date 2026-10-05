#pragma once
#include <gtk/gtk.h>
G_BEGIN_DECLS
/* A zero dimension fills the parent's allocation; a positive one is fixed. */
GtkWidget *anto_size_bin_new(GtkWidget *child, int width, int height);
void anto_size_bin_set_size(GtkWidget *bin, int width, int height);
G_END_DECLS
