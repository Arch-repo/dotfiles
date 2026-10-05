#include "menu.h"
#include "ui_internal.h"
#include "local_config.h"
#include <gtk4-layer-shell.h>
#include <json-c/json.h>
#include <stdlib.h>
#include <string.h>
static gboolean search_matches(MenuApp *app, GObject *object) {
    const char *query = gtk_editable_get_text(GTK_EDITABLE(app->search));
    if (!query || !*query) return TRUE;
    const char *haystack = g_object_get_data(object, "menu-search");
    if (!haystack) return TRUE;
    g_autofree char *needle = g_utf8_strdown(query, -1);
    g_auto(GStrv) tokens = g_strsplit(needle, " ", -1);
    for (guint i = 0; tokens[i]; i++) {
        if (!*tokens[i]) continue;
        if (!strstr(haystack, tokens[i])) return FALSE;
    }
    return TRUE;
}

gboolean row_filter(GtkListBoxRow *row, gpointer data) {
    return search_matches(data, G_OBJECT(row));
}

gboolean tile_filter(GtkFlowBoxChild *child, gpointer data) {
    return search_matches(data, G_OBJECT(child));
}

static gboolean menu_item_is_visible(GtkWidget *widget) {
    return widget && gtk_widget_get_visible(widget) &&
           gtk_widget_get_child_visible(widget);
}

static GtkListBoxRow *first_visible_row(MenuApp *app) {
    for (int i = 0;; i++) {
        GtkListBoxRow *row = gtk_list_box_get_row_at_index(GTK_LIST_BOX(app->list), i);
        if (!row) return NULL;
        if (menu_item_is_visible(GTK_WIDGET(row)) &&
            gtk_list_box_row_get_selectable(row))
            return row;
    }
}

static GtkFlowBoxChild *first_visible_tile(MenuApp *app) {
    for (int i = 0;; i++) {
        GtkFlowBoxChild *child = gtk_flow_box_get_child_at_index(GTK_FLOW_BOX(app->grid), i);
        if (!child) return NULL;
        if (menu_item_is_visible(GTK_WIDGET(child))) return child;
    }
}

void search_changed(GtkSearchEntry *entry, gpointer data) {
    MenuApp *app = data;
    if (app->search_action) {
        app->search_action(app, gtk_editable_get_text(GTK_EDITABLE(entry)),
                           app->search_data);
    } else {
        gtk_list_box_invalidate_filter(GTK_LIST_BOX(app->list));
        gtk_flow_box_invalidate_filter(GTK_FLOW_BOX(app->grid));
    }
    if (app->layout == MENU_LAYOUT_GRID) {
        GtkFlowBoxChild *first = first_visible_tile(app);
        if (first)
            gtk_flow_box_select_child(GTK_FLOW_BOX(app->grid), first);
        else
            gtk_flow_box_unselect_all(GTK_FLOW_BOX(app->grid));
    } else {
        GtkListBoxRow *first = first_visible_row(app);
        gtk_list_box_select_row(GTK_LIST_BOX(app->list), first);
    }
}

void row_activated(GtkListBox *box, GtkListBoxRow *row, gpointer data) {
    (void)box;
    (void)data;
    RowAction *action = g_object_get_data(G_OBJECT(row), "menu-action");
    if (action && action->callback) action->callback(action->app, action->data);
}

void tile_activated(GtkFlowBox *box, GtkFlowBoxChild *child, gpointer data) {
    (void)box;
    (void)data;
    RowAction *action = g_object_get_data(G_OBJECT(child), "menu-action");
    if (action && action->callback) action->callback(action->app, action->data);
}

static GtkListBoxRow *next_visible_row(MenuApp *app, GtkListBoxRow *current, int direction) {
    int start = current ? gtk_list_box_row_get_index(current) : (direction > 0 ? -1 : 99999);
    for (int i = start + direction; i >= 0 && i < 10000; i += direction) {
        GtkListBoxRow *row = gtk_list_box_get_row_at_index(GTK_LIST_BOX(app->list), i);
        if (!row) break;
        if (menu_item_is_visible(GTK_WIDGET(row)) &&
            gtk_list_box_row_get_selectable(row))
            return row;
    }
    return current;
}

static GtkFlowBoxChild *move_tile_selection(MenuApp *app, int delta) {
    GList *selected = gtk_flow_box_get_selected_children(GTK_FLOW_BOX(app->grid));
    GtkFlowBoxChild *current = selected ? selected->data : NULL;
    g_list_free(selected);

    g_autoptr(GPtrArray) visible = g_ptr_array_new();
    int current_position = -1;
    for (int i = 0;; i++) {
        GtkFlowBoxChild *child = gtk_flow_box_get_child_at_index(GTK_FLOW_BOX(app->grid), i);
        if (!child) break;
        if (!menu_item_is_visible(GTK_WIDGET(child))) continue;
        if (child == current) current_position = (int)visible->len;
        g_ptr_array_add(visible, child);
    }
    if (!visible->len) return NULL;
    if (current_position < 0) return g_ptr_array_index(visible, 0);
    int next = CLAMP(current_position + delta, 0, (int)visible->len - 1);
    return g_ptr_array_index(visible, next);
}

gboolean key_pressed(GtkEventControllerKey *controller, guint keyval,
                            guint keycode, GdkModifierType state, gpointer data) {
    (void)controller;
    (void)keycode;
    MenuApp *app = data;
    if (keyval == GDK_KEY_Escape) {
        menu_close(app);
        return TRUE;
    }
    if ((state & GDK_CONTROL_MASK) && (keyval == GDK_KEY_BackSpace || keyval == GDK_KEY_h)) {
        menu_back(app);
        return TRUE;
    }
    if (app->key_action && app->key_action(keyval, state, app->key_data))
        return TRUE;

    GtkWidget *focus = gtk_window_get_focus(app->window);
    if (focus && focus != app->search && !gtk_widget_is_ancestor(focus, app->search) &&
        (GTK_IS_EDITABLE(focus) || GTK_IS_RANGE(focus) || GTK_IS_TEXT_VIEW(focus) ||
         GTK_IS_BUTTON(focus) || GTK_IS_SWITCH(focus) || GTK_IS_CALENDAR(focus)))
        return FALSE;

    if (keyval == GDK_KEY_Tab && (focus == app->search || (focus && gtk_widget_is_ancestor(focus, app->search)))) {
        if (app->layout == MENU_LAYOUT_GRID) {
            GtkFlowBoxChild *first = first_visible_tile(app);
            if (first) {
                gtk_flow_box_select_child(GTK_FLOW_BOX(app->grid), first);
                gtk_widget_grab_focus(GTK_WIDGET(first));
                return TRUE;
            }
        } else if (app->layout == MENU_LAYOUT_LIST) {
            GtkListBoxRow *first = first_visible_row(app);
            if (first) {
                gtk_list_box_select_row(GTK_LIST_BOX(app->list), first);
                gtk_widget_grab_focus(GTK_WIDGET(first));
                return TRUE;
            }
        }
    }

    if (app->layout == MENU_LAYOUT_GRID &&
        (keyval == GDK_KEY_Down || keyval == GDK_KEY_Up ||
         keyval == GDK_KEY_Left || keyval == GDK_KEY_Right)) {
        int delta = 0;
        if (keyval == GDK_KEY_Left) delta = -1;
        if (keyval == GDK_KEY_Right) delta = 1;
        if (keyval == GDK_KEY_Up) delta = -(int)app->grid_columns;
        if (keyval == GDK_KEY_Down) delta = (int)app->grid_columns;
        GtkFlowBoxChild *next = move_tile_selection(app, delta);
        if (next) {
            gtk_flow_box_select_child(GTK_FLOW_BOX(app->grid), next);
            gtk_widget_grab_focus(GTK_WIDGET(next));
        }
        return TRUE;
    }

    if (app->layout == MENU_LAYOUT_LIST && (keyval == GDK_KEY_Down || keyval == GDK_KEY_Up)) {
        GtkListBoxRow *current = gtk_list_box_get_selected_row(GTK_LIST_BOX(app->list));
        GtkListBoxRow *next = next_visible_row(app, current, keyval == GDK_KEY_Down ? 1 : -1);
        if (next) {
            gtk_list_box_select_row(GTK_LIST_BOX(app->list), next);
            gtk_widget_grab_focus(GTK_WIDGET(next));
        }
        return TRUE;
    }

    if (keyval == GDK_KEY_Return || keyval == GDK_KEY_KP_Enter) {
        if (app->layout == MENU_LAYOUT_GRID) {
            GList *selected = gtk_flow_box_get_selected_children(GTK_FLOW_BOX(app->grid));
            GtkFlowBoxChild *child = selected ? selected->data : NULL;
            g_list_free(selected);
            if (child && menu_item_is_visible(GTK_WIDGET(child))) {
                g_signal_emit_by_name(app->grid, "child-activated", child);
                return TRUE;
            }
        } else {
            GtkListBoxRow *row = gtk_list_box_get_selected_row(GTK_LIST_BOX(app->list));
            if (row && menu_item_is_visible(GTK_WIDGET(row)) &&
                gtk_list_box_row_get_selectable(row)) {
                g_signal_emit_by_name(app->list, "row-activated", row);
                return TRUE;
            }
        }
    }
    return FALSE;
}
