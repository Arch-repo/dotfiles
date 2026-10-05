#include "internal.h"
#include "primitives.h"

const char *anto_display_output_icon(const DisplayMonitor *monitor) {
    if (anto_display_output_is_internal(monitor->name)) return "computer-laptop-symbolic";
    return monitor->enabled ? "video-display-symbolic" : "video-display-symbolic";
}

DisplayRow *anto_display_row_new(MenuApp *app, DisplayRuntime *runtime, const char *key, DisplaySection section) {
    DisplayRow *row = g_new0(DisplayRow, 1);
    row->key = g_strdup(key); row->section = section; row->runtime = runtime;
    row->row = gtk_list_box_row_new();
    gtk_widget_add_css_class(row->row, "setting-item");
    g_object_set_data(G_OBJECT(row->row), "display-key", row->key);
    row->icon = anto_ui_icon("video-display-symbolic", ANTO_CONTROL_ROW_ICON, "item-icon");
    GtkWidget *body = anto_ui_row(row->icon, "", "", &row->title, &row->subtitle);
    row->badge = anto_ui_badge("");
    gtk_widget_add_css_class(row->badge, "item-badge");
    gtk_box_append(GTK_BOX(body), row->badge);
    gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row->row), body);
    row->action.callback = anto_display_row_run;
    row->action.app = app; row->action.data = row; row->action.destroy = NULL;
    gtk_list_box_append(GTK_LIST_BOX(app->list), row->row);
    return row;
}

GtkWidget *anto_display_section_row(MenuApp *app, const char *key,
                                      const char *title) {
    GtkWidget *row = gtk_list_box_row_new();
    gtk_list_box_row_set_selectable(GTK_LIST_BOX_ROW(row), FALSE);
    gtk_list_box_row_set_activatable(GTK_LIST_BOX_ROW(row), FALSE);
    GtkWidget *label = anto_ui_section(title);
    gtk_label_set_xalign(GTK_LABEL(label), 0);
    gtk_widget_add_css_class(label, "section-title");
    gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), label);
    g_object_set_data(G_OBJECT(row), "display-key", (gpointer)key);
    gtk_list_box_append(GTK_LIST_BOX(app->list), row);
    return row;
}

DisplayArranger *anto_display_add_display_arranger(MenuApp *app) {
    DisplayArranger *a = g_new0(DisplayArranger, 1);
    a->app = app; a->selected = -1;
    a->monitors = g_ptr_array_new_with_free_func(anto_display_arrange_monitor_free);
    GtkWidget *card = anto_ui_stack(GTK_ORIENTATION_VERTICAL, ANTO_SPACING_MD, "ui-summary");
    gtk_box_append(GTK_BOX(card), anto_ui_copy("Disponi gli schermi", "Trascina un display per spostarlo", &a->title_label, &a->detail_label));
    a->area = gtk_drawing_area_new();
    gtk_widget_add_css_class(a->area, "display-arranger");
    gtk_widget_set_size_request(a->area, -1, 220);
    gtk_widget_set_hexpand(a->area, TRUE);
    gtk_widget_set_cursor_from_name(a->area, "grab");
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(a->area), anto_display_arranger_draw, a, NULL);
    GtkGesture *drag = gtk_gesture_drag_new();
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(drag), GDK_BUTTON_PRIMARY);
    g_signal_connect(drag, "drag-begin", G_CALLBACK(anto_display_arranger_drag_begin), a);
    g_signal_connect(drag, "drag-update", G_CALLBACK(anto_display_arranger_drag_update), a);
    g_signal_connect(drag, "drag-end", G_CALLBACK(anto_display_arranger_drag_end), a);
    gtk_widget_add_controller(a->area, GTK_EVENT_CONTROLLER(drag));
    gtk_box_append(GTK_BOX(card), a->area);
    GtkWidget *footer = anto_ui_stack(GTK_ORIENTATION_HORIZONTAL, ANTO_SPACING_SM, NULL);
    a->selection_label = anto_ui_text("", "ui-caption", 2);
    gtk_widget_set_hexpand(a->selection_label, TRUE);
    gtk_box_append(GTK_BOX(footer), a->selection_label);
    a->reset_button = anto_ui_action("Ripristina", "edit-undo-symbolic", NULL);
    a->apply_button = anto_ui_action("Applica", "object-select-symbolic", "primary");
    g_signal_connect(a->reset_button, "clicked", G_CALLBACK(anto_display_arranger_reset_clicked), a);
    g_signal_connect(a->apply_button, "clicked", G_CALLBACK(anto_display_arranger_apply_clicked), a);
    gtk_box_append(GTK_BOX(footer), a->reset_button);
    gtk_box_append(GTK_BOX(footer), a->apply_button);
    gtk_box_append(GTK_BOX(card), footer);
    anto_display_arranger_update_controls(a);
    anto_display_active_arranger = a;
    g_object_set_data_full(G_OBJECT(card), "display-arranger", a, anto_display_arranger_free);
    menu_append_widget(app, card);
    a->row = gtk_widget_get_last_child(app->list);
    g_object_set_data(G_OBJECT(a->row), "display-key", "fixed:arranger");
    return a;
}

DisplayView *anto_display_build_page(MenuApp *app,
                                       DisplayRuntime *runtime) {
    menu_page_begin(app, "video-display-symbolic", "Schermi",
                    "Disposizione, risoluzione e profili",
                    "Cerca monitor, virtuali, layout, scala o frequenza…");
    DisplayView *view = g_new0(DisplayView, 1);
    view->app = app;
    view->runtime = runtime;
    view->rows = g_hash_table_new_full(
        g_str_hash, g_str_equal, g_free, anto_display_row_free);
    runtime->view = view;

    view->loading = anto_display_row_new(
        app, runtime, "fixed:loading", DISPLAY_SECTION_CONNECTED);
    DisplayRowSpec loading = {
        .key = "fixed:loading",
        .section = DISPLAY_SECTION_CONNECTED,
        .icon = "content-loading-symbolic",
        .title = "Aggiornamento schermi",
        .subtitle =
            "Monitor, modalità e profili vengono letti senza bloccare il menu",
        .badge = "LIVE",
    };
    anto_display_row_update(view->loading, &loading);

    view->arranger_header = anto_display_section_row(
        app, "fixed:section:arranger", "DISPOSIZIONE VISIVA");
    view->arranger = anto_display_add_display_arranger(app);
    gtk_widget_set_visible(view->arranger_header, FALSE);
    gtk_widget_set_visible(view->arranger->row, FALSE);
    for (guint section = 0; section < DISPLAY_SECTION_COUNT; section++) {
        view->section_headers[section] = anto_display_section_row(
            app, anto_display_section_keys[section], anto_display_section_titles[section]);
        gtk_widget_set_visible(view->section_headers[section], FALSE);
    }
    menu_set_search_action(app, anto_display_search, view, anto_display_view_free);
    menu_set_footer(
        app, "Snapshot in corso · nessuna configurazione viene applicata");
    return view;
}

void menu_show_display(MenuApp *app) {
    DisplayRuntime *runtime = anto_display_runtime_get(app);
    if (!runtime) return;
    anto_display_build_page(app, runtime);
    if (runtime->snapshot) {
        runtime->snapshot_dirty = TRUE;
        anto_display_render_if_safe(runtime);
    }
    anto_display_refresh_start(runtime);
}
