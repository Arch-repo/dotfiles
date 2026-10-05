#pragma once
#include "menu.h"
typedef struct { MenuAction callback; MenuApp *app; gpointer data; GDestroyNotify destroy; } RowAction;
typedef struct { MenuScaleAction callback; MenuApp *app; gpointer data; GDestroyNotify destroy; } ScaleAction;
gboolean row_filter(GtkListBoxRow*,gpointer);
gboolean tile_filter(GtkFlowBoxChild*,gpointer);
void search_changed(GtkSearchEntry*,gpointer);
void row_activated(GtkListBox*,GtkListBoxRow*,gpointer);
void tile_activated(GtkFlowBox*,GtkFlowBoxChild*,gpointer);
gboolean key_pressed(GtkEventControllerKey*,guint,guint,GdkModifierType,gpointer);
void load_css(void);
void update_rail_state(MenuApp*);
