#include "menu.h"
#include "ui_internal.h"
#include "primitives.h"
#include "components.h"

void menu_set_layout(MenuApp *app, MenuLayout layout) {
    app->layout = layout;
    const char *name = "list";
    if (layout == MENU_LAYOUT_GRID) name = "grid";
    if (layout == MENU_LAYOUT_CUSTOM) name = "custom";
    gtk_stack_set_visible_child_name(GTK_STACK(app->content_stack), name);
}

void menu_set_grid_columns(MenuApp *app, guint columns) {
    app->grid_columns = MAX(columns, 1u);
    gtk_flow_box_set_min_children_per_line(GTK_FLOW_BOX(app->grid), app->grid_columns);
    gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(app->grid), app->grid_columns);
}

void menu_set_custom_content(MenuApp *app, GtkWidget *widget) {
    GtkWidget *child;
    while ((child = gtk_widget_get_first_child(app->custom_holder)) != NULL)
        gtk_box_remove(GTK_BOX(app->custom_holder), child);
    if (widget) gtk_box_append(GTK_BOX(app->custom_holder), widget);
    menu_set_layout(app, MENU_LAYOUT_CUSTOM);
}

void menu_set_search_action(MenuApp *app, MenuSearchAction action,
                            gpointer data, GDestroyNotify destroy) {
    app->search_action = NULL;
    app->search_data = NULL;
    g_object_set_data_full(G_OBJECT(app->search), "menu-search-action-data", NULL, NULL);
    app->search_action = action;
    app->search_data = data;
    if (data)
        g_object_set_data_full(G_OBJECT(app->search), "menu-search-action-data",
                               data, destroy);
}

void menu_page_begin(MenuApp *app, const char *icon, const char *title,
                     const char *subtitle, const char *placeholder) {
    GtkWidget *child;
    app->search_action = NULL;
    app->search_data = NULL;
    app->key_action = NULL;
    app->key_data = NULL;
    while ((child = gtk_widget_get_first_child(app->custom_holder)) != NULL)
        gtk_box_remove(GTK_BOX(app->custom_holder), child);
    g_object_set_data_full(G_OBJECT(app->search), "menu-search-action-data", NULL, NULL);
    while ((child = gtk_widget_get_first_child(app->list)) != NULL)
        gtk_list_box_remove(GTK_LIST_BOX(app->list), child);
    while ((child = gtk_widget_get_first_child(app->grid)) != NULL)
        gtk_flow_box_remove(GTK_FLOW_BOX(app->grid), child);

    menu_set_grid_columns(app, 3);
    gtk_widget_remove_css_class(app->grid, "overview-grid");
    menu_set_layout(app, MENU_LAYOUT_LIST);
    gtk_image_set_from_icon_name(GTK_IMAGE(app->page_icon), icon);
    gtk_label_set_text(GTK_LABEL(app->page_title), title);
    gtk_label_set_text(GTK_LABEL(app->page_subtitle), subtitle ? subtitle : "");
    gtk_widget_set_visible(app->page_subtitle, subtitle && *subtitle);
    gtk_label_set_text(GTK_LABEL(app->context_page), title ? title : "MENU");
    gtk_search_entry_set_placeholder_text(GTK_SEARCH_ENTRY(app->search), placeholder ? placeholder : "Cerca…");
    gtk_editable_set_text(GTK_EDITABLE(app->search), "");
    gtk_widget_set_visible(app->search, TRUE);
    gtk_widget_set_visible(app->back_button, app->history && app->history->len > 0);
    update_rail_state(app);
    menu_set_footer(app, "↑↓ naviga  ·  Invio apri  ·  Esc chiude");
}

void menu_add_section(MenuApp *app, const char *title) {
    GtkWidget *row = menu_ui_static_row(anto_ui_section(title));
    gtk_widget_add_css_class(row, "section-row");
    gtk_list_box_append(GTK_LIST_BOX(app->list), row);
}
void menu_append_widget(MenuApp *app, GtkWidget *widget) {
    GtkWidget *row = menu_ui_static_row(widget);
    const char *search = g_object_get_data(G_OBJECT(widget), "menu-search");
    if (search) g_object_set_data_full(G_OBJECT(row), "menu-search", g_strdup(search), g_free);
    gtk_list_box_append(GTK_LIST_BOX(app->list), row);
}
void menu_set_footer(MenuApp *app, const char *text) {
    gtk_label_set_text(GTK_LABEL(app->footer), text ? text : "");
}
void menu_close(MenuApp *app) {
    if (app && app->window) gtk_window_close(app->window);
}
