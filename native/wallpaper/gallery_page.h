#pragma once
#include <gtk/gtk.h>

G_BEGIN_DECLS
typedef void (*AntoWallpaperNavigate)(const char *page, gpointer data);
GtkWidget *anto_wallpaper_page_new(GtkWindow *window, GtkWidget *search,
                                   int width, AntoWallpaperNavigate navigate,
                                   gpointer data);
void anto_wallpaper_page_search(GtkWidget *page, const char *query);
gboolean anto_wallpaper_page_key(GtkWidget *page, guint key,
                                GdkModifierType modifiers);
G_END_DECLS
