#pragma once
#include "shell.h"
GtkWidget *menu_ui_static_row(GtkWidget *child);
GtkWidget *menu_ui_item_icon(const char *icon, int size);
void menu_ui_bind(GtkWidget *widget, MenuApp *app, MenuAction callback, gpointer data, GDestroyNotify destroy);
void menu_ui_search(GtkWidget *widget, const char *title, const char *subtitle, const char *badge);
