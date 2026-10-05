#include "internal.h"
#include "primitives.h"

void anto_display_row_run(MenuApp *app, gpointer data) {
    DisplayRow *row = data;
    if (!row || !row->command || !*row->command) return;
    if (row->section != DISPLAY_SECTION_VIRTUAL || !row->runtime) {
        menu_spawn_shell(app, row->command, row->close_after);
        return;
    }

    g_autoptr(GError) error = NULL;
    GSubprocess *process = g_subprocess_new(
        G_SUBPROCESS_FLAGS_STDOUT_PIPE | G_SUBPROCESS_FLAGS_STDERR_PIPE,
        &error, "/bin/sh", "-lc", row->command, NULL);
    if (!process) {
        menu_notify("Monitor virtuali",
                    error ? error->message : "Impossibile avviare il backend");
        return;
    }
    g_subprocess_communicate_utf8_async(
        process, NULL, NULL, anto_display_virtual_command_finished,
        anto_display_runtime_ref(row->runtime));
    g_object_unref(process);
}

void anto_display_row_update(DisplayRow *row,
                               const DisplayRowSpec *spec) {
    row->section = spec->section;
    row->available = !spec->hidden;
    if (spec->section == DISPLAY_SECTION_VIRTUAL)
        gtk_widget_add_css_class(row->row, "display-virtual-row");
    else
        gtk_widget_remove_css_class(row->row, "display-virtual-row");
    if (g_str_has_prefix(spec->key, "virtual:preset:"))
        gtk_widget_add_css_class(row->row, "display-virtual-preset");
    else
        gtk_widget_remove_css_class(row->row, "display-virtual-preset");
    if (g_str_has_suffix(spec->key, ":summary"))
        gtk_widget_add_css_class(row->row, "display-virtual-summary");
    else
        gtk_widget_remove_css_class(row->row, "display-virtual-summary");
    if (g_str_has_suffix(spec->key, ":remove"))
        gtk_widget_add_css_class(row->row, "danger-row");
    else
        gtk_widget_remove_css_class(row->row, "danger-row");
    const char *icon = spec->icon && *spec->icon
                           ? spec->icon
                           : "application-x-executable-symbolic";
    const char *title = spec->title ? spec->title : "";
    const char *subtitle = spec->subtitle ? spec->subtitle : "";
    const char *badge = spec->badge ? spec->badge : "";
    if (g_strcmp0(row->icon_name, icon) != 0) {
        gtk_image_set_from_icon_name(GTK_IMAGE(row->icon), icon);
        g_free(row->icon_name);
        row->icon_name = g_strdup(icon);
    }
    if (g_strcmp0(row->title_text, title) != 0) {
        gtk_label_set_text(GTK_LABEL(row->title), title);
        g_free(row->title_text);
        row->title_text = g_strdup(title);
    }
    if (g_strcmp0(row->subtitle_text, subtitle) != 0) {
        gtk_label_set_text(GTK_LABEL(row->subtitle), subtitle);
        g_free(row->subtitle_text);
        row->subtitle_text = g_strdup(subtitle);
    }
    gtk_widget_set_visible(row->subtitle,
                           spec->subtitle && *spec->subtitle);
    if (g_strcmp0(row->badge_text, badge) != 0) {
        gtk_label_set_text(GTK_LABEL(row->badge), badge);
        g_free(row->badge_text);
        row->badge_text = g_strdup(badge);
    }
    gtk_widget_set_visible(row->badge, spec->badge && *spec->badge);

    if (g_strcmp0(row->command, spec->command) != 0) {
        g_free(row->command);
        row->command = g_strdup(spec->command);
    }
    row->close_after = spec->close_after;
    gboolean actionable = row->command && *row->command;
    gtk_list_box_row_set_activatable(GTK_LIST_BOX_ROW(row->row), actionable);
    gtk_list_box_row_set_selectable(GTK_LIST_BOX_ROW(row->row), actionable);
    if (actionable) {
        gtk_widget_add_css_class(row->row, "activatable");
        g_object_set_data(G_OBJECT(row->row), "menu-action", &row->action);
    } else {
        gtk_widget_remove_css_class(row->row, "activatable");
        g_object_set_data(G_OBJECT(row->row), "menu-action", NULL);
    }

    g_autofree char *combined = g_strdup_printf(
        "%s %s %s", spec->title ? spec->title : "",
        spec->subtitle ? spec->subtitle : "",
        spec->badge ? spec->badge : "");
    g_autofree char *search_text = g_utf8_strdown(combined, -1);
    if (g_strcmp0(row->search_text, search_text) != 0) {
        g_free(row->search_text);
        row->search_text = g_strdup(search_text);
        g_object_set_data_full(G_OBJECT(row->row), "menu-search",
                               g_strdup(row->search_text), g_free);
    }
    gtk_widget_set_visible(row->row, row->available);
}

void anto_display_arranger_compute_view(DisplayArranger *arranger,
                                  int canvas_width, int canvas_height) {
    double min_x = G_MAXDOUBLE;
    double min_y = G_MAXDOUBLE;
    double max_x = -G_MAXDOUBLE;
    double max_y = -G_MAXDOUBLE;
    for (guint i = 0; i < arranger->monitors->len; i++) {
        ArrangeMonitor *monitor = g_ptr_array_index(arranger->monitors, i);
        min_x = MIN(min_x, monitor->x);
        min_y = MIN(min_y, monitor->y);
        max_x = MAX(max_x, monitor->x + monitor->width);
        max_y = MAX(max_y, monitor->y + monitor->height);
    }
    if (arranger->monitors->len == 0) {
        arranger->view_scale = 1.0;
        arranger->view_offset_x = 0.0;
        arranger->view_offset_y = 0.0;
        return;
    }

    double world_width = MAX(max_x - min_x, 1.0);
    double world_height = MAX(max_y - min_y, 1.0);
    double available_width = MAX(canvas_width - 44.0, 1.0);
    double available_height = MAX(canvas_height - 42.0, 1.0);
    arranger->view_scale = MIN(available_width / world_width,
                               available_height / world_height);
    if (!isfinite(arranger->view_scale) || arranger->view_scale <= 0.0)
        arranger->view_scale = 1.0;
    arranger->view_offset_x = (canvas_width - world_width * arranger->view_scale) / 2.0 -
                              min_x * arranger->view_scale;
    arranger->view_offset_y = (canvas_height - world_height * arranger->view_scale) / 2.0 -
                              min_y * arranger->view_scale;
}

void anto_display_arranger_view_rect(const DisplayArranger *arranger,
                               const ArrangeMonitor *monitor,
                               double *x, double *y,
                               double *width, double *height) {
    *x = arranger->view_offset_x + monitor->x * arranger->view_scale;
    *y = arranger->view_offset_y + monitor->y * arranger->view_scale;
    *width = monitor->width * arranger->view_scale;
    *height = monitor->height * arranger->view_scale;
}

gboolean anto_display_restore_scroll(gpointer data) {
    DisplayScrollRestore *restore = data;
    GObject *window = g_weak_ref_get(&restore->window);
    if (window && g_strcmp0(restore->app->current_page, "display") == 0) {
        double lower = gtk_adjustment_get_lower(restore->adjustment);
        double maximum = MAX(
            lower, gtk_adjustment_get_upper(restore->adjustment) -
                       gtk_adjustment_get_page_size(restore->adjustment));
        gtk_adjustment_set_value(
            restore->adjustment, CLAMP(restore->value, lower, maximum));
    }
    g_clear_object(&window);
    return G_SOURCE_REMOVE;
}

void anto_display_schedule_scroll_restore(MenuApp *app, double value) {
    DisplayScrollRestore *restore = g_new0(DisplayScrollRestore, 1);
    restore->app = app;
    restore->adjustment = g_object_ref(
        gtk_scrolled_window_get_vadjustment(
            GTK_SCROLLED_WINDOW(app->list_scroll)));
    restore->value = value;
    g_weak_ref_init(&restore->window, G_OBJECT(app->window));
    g_idle_add_full(G_PRIORITY_DEFAULT_IDLE, anto_display_restore_scroll, restore,
                    anto_display_scroll_restore_free);
}

void anto_display_search(MenuApp *app, const char *query, gpointer data) {
    (void)app;
    DisplayView *view = data;
    g_free(view->query);
    view->query = g_utf8_strdown(query ? query : "", -1);
    guint visible[DISPLAY_SECTION_COUNT] = {0};

    GHashTableIter iterator;
    gpointer value = NULL;
    g_hash_table_iter_init(&iterator, view->rows);
    while (g_hash_table_iter_next(&iterator, NULL, &value)) {
        DisplayRow *row = value;
        gboolean matches = row->available && (!*view->query ||
            (row->search_text &&
             strstr(row->search_text, view->query) != NULL));
        gtk_widget_set_visible(row->row, matches);
        if (matches) visible[row->section]++;
    }
    for (guint section = 0; section < DISPLAY_SECTION_COUNT; section++)
        gtk_widget_set_visible(view->section_headers[section],
                               view->ready && visible[section] > 0);

    gboolean arranger_matches = !*view->query ||
        strstr("disponi schermi monitor layout trascina posizione",
               view->query) != NULL;
    gboolean arranger_visible =
        view->ready && view->arranger->monitors->len > 0 &&
        arranger_matches;
    gtk_widget_set_visible(view->arranger_header, arranger_visible);
    gtk_widget_set_visible(view->arranger->row, arranger_visible);
    gtk_widget_set_visible(view->loading->row, !view->ready);

    GtkListBox *list = GTK_LIST_BOX(view->app->list);
    GtkListBoxRow *selected = gtk_list_box_get_selected_row(list);
    if (selected && !gtk_widget_get_visible(GTK_WIDGET(selected)))
        gtk_list_box_select_row(list,
                                anto_display_first_visible_selectable(view));
}

void anto_display_reorder_rows(DisplayView *view, GPtrArray *specs) {
    g_autoptr(GPtrArray) desired = g_ptr_array_new();
    g_ptr_array_add(desired, view->loading->row);
    g_ptr_array_add(desired, view->arranger_header);
    g_ptr_array_add(desired, view->arranger->row);
    for (guint section = 0; section < DISPLAY_SECTION_COUNT; section++) {
        g_ptr_array_add(desired, view->section_headers[section]);
        for (guint i = 0; i < specs->len; i++) {
            DisplayRowSpec *spec = g_ptr_array_index(specs, i);
            if (spec->section != (DisplaySection)section) continue;
            DisplayRow *row = g_hash_table_lookup(view->rows, spec->key);
            if (row) g_ptr_array_add(desired, row->row);
        }
    }

    for (guint index = 0; index < desired->len; index++) {
        GtkWidget *wanted = g_ptr_array_index(desired, index);
        GtkListBoxRow *current = gtk_list_box_get_row_at_index(
            GTK_LIST_BOX(view->app->list), (int)index);
        if (GTK_WIDGET(current) == wanted) continue;
        g_object_ref(wanted);
        gtk_list_box_remove(GTK_LIST_BOX(view->app->list), wanted);
        gtk_list_box_insert(GTK_LIST_BOX(view->app->list), wanted,
                            (int)index);
        g_object_unref(wanted);
    }
}

void anto_display_reconcile_rows(DisplayView *view, GPtrArray *specs) {
    g_autoptr(GHashTable) wanted = g_hash_table_new(
        g_str_hash, g_str_equal);
    for (guint i = 0; i < specs->len; i++) {
        DisplayRowSpec *spec = g_ptr_array_index(specs, i);
        g_hash_table_add(wanted, spec->key);
        DisplayRow *row = g_hash_table_lookup(view->rows, spec->key);
        if (!row) {
            row = anto_display_row_new(
                view->app, view->runtime, spec->key, spec->section);
            g_hash_table_insert(view->rows, g_strdup(spec->key), row);
        }
        anto_display_row_update(row, spec);
    }

    GHashTableIter iterator;
    gpointer key = NULL;
    gpointer value = NULL;
    g_hash_table_iter_init(&iterator, view->rows);
    while (g_hash_table_iter_next(&iterator, &key, &value)) {
        if (g_hash_table_contains(wanted, key)) continue;
        DisplayRow *row = value;
        gtk_list_box_remove(GTK_LIST_BOX(view->app->list), row->row);
        g_hash_table_iter_remove(&iterator);
    }
    anto_display_reorder_rows(view, specs);
}

gboolean anto_display_render_if_safe(DisplayRuntime *runtime) {
    if (!runtime || !runtime->snapshot ||
        g_strcmp0(runtime->app->current_page, "display") != 0)
        return FALSE;
    DisplayArranger *arranger = anto_display_unsafe_arranger(runtime->app);
    if (arranger) {
        arranger->refresh_pending = TRUE;
        return FALSE;
    }
    anto_display_apply_snapshot_in_place(runtime);
    return TRUE;
}
