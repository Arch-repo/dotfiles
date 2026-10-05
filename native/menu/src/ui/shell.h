#pragma once
#include "menu.h"
#include "primitives.h"
#include "ui_internal.h"
GtkWidget *menu_ui_sidebar(MenuApp *app);
GtkWidget *menu_ui_header(MenuApp *app);
GtkWidget *menu_ui_content(MenuApp *app);
GtkWidget *menu_ui_status(MenuApp *app);
void menu_context_start(MenuApp *app);
