#include "shell.h"
GtkWidget *menu_ui_content(MenuApp *app) {
    app->content_stack = gtk_stack_new();
    gtk_stack_set_hhomogeneous(GTK_STACK(app->content_stack), FALSE);
    gtk_stack_set_vhomogeneous(GTK_STACK(app->content_stack), FALSE);
    gtk_stack_set_transition_type(GTK_STACK(app->content_stack), GTK_STACK_TRANSITION_TYPE_CROSSFADE);
    gtk_stack_set_transition_duration(GTK_STACK(app->content_stack), 120);
    gtk_widget_set_vexpand(app->content_stack, TRUE);
    gtk_widget_set_overflow(app->content_stack, GTK_OVERFLOW_HIDDEN);
    app->list = gtk_list_box_new();
    gtk_widget_add_css_class(app->list, "menu-list");
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(app->list), GTK_SELECTION_SINGLE);
    gtk_list_box_set_filter_func(GTK_LIST_BOX(app->list), row_filter, app, NULL);
    g_signal_connect(app->list, "row-activated", G_CALLBACK(row_activated), app);
    app->list_scroll = anto_ui_scroller(app->list);
    gtk_stack_add_named(GTK_STACK(app->content_stack), app->list_scroll, "list");
    app->grid = gtk_flow_box_new();
    gtk_widget_add_css_class(app->grid, "menu-grid");
    gtk_widget_set_valign(app->grid, GTK_ALIGN_START);
    gtk_widget_set_vexpand(app->grid, FALSE);
    gtk_flow_box_set_selection_mode(GTK_FLOW_BOX(app->grid), GTK_SELECTION_SINGLE);
    gtk_flow_box_set_homogeneous(GTK_FLOW_BOX(app->grid), TRUE);
    gtk_flow_box_set_row_spacing(GTK_FLOW_BOX(app->grid), ANTO_SPACING_MD);
    gtk_flow_box_set_column_spacing(GTK_FLOW_BOX(app->grid), ANTO_SPACING_MD);
    gtk_flow_box_set_filter_func(GTK_FLOW_BOX(app->grid), tile_filter, app, NULL);
    g_signal_connect(app->grid, "child-activated", G_CALLBACK(tile_activated), app);
    app->grid_scroll = anto_ui_scroller(app->grid);
    gtk_stack_add_named(GTK_STACK(app->content_stack), app->grid_scroll, "grid");
    app->custom_holder = anto_ui_stack(GTK_ORIENTATION_VERTICAL, 0, "custom-content");
    gtk_widget_set_vexpand(app->custom_holder, TRUE);
    gtk_widget_set_overflow(app->custom_holder, GTK_OVERFLOW_HIDDEN);
    gtk_stack_add_named(GTK_STACK(app->content_stack), app->custom_holder, "custom");
    return app->content_stack;
}
