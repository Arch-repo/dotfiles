#include "internal.h"

DisplayArranger *anto_display_active_arranger = NULL;
const char *const anto_display_section_titles[DISPLAY_SECTION_COUNT] = {
    [DISPLAY_SECTION_CONNECTED] = "SCHERMI COLLEGATI",
    [DISPLAY_SECTION_VIRTUAL] = "MONITOR VIRTUALI",
    [DISPLAY_SECTION_LAYOUT] = "MODALITÀ DISPLAY",
    [DISPLAY_SECTION_SCALE] = "SCALA",
    [DISPLAY_SECTION_TRANSFORM] = "ORIENTAMENTO",
    [DISPLAY_SECTION_MODE] = "RISOLUZIONE E FREQUENZA",
    [DISPLAY_SECTION_PROFILE] = "PROFILI PERSISTENTI",
    [DISPLAY_SECTION_TOOL] = "STRUMENTI",
};
const char *const anto_display_section_keys[DISPLAY_SECTION_COUNT] = {
    [DISPLAY_SECTION_CONNECTED] = "fixed:section:connected",
    [DISPLAY_SECTION_VIRTUAL] = "fixed:section:virtual",
    [DISPLAY_SECTION_LAYOUT] = "fixed:section:layout",
    [DISPLAY_SECTION_SCALE] = "fixed:section:scale",
    [DISPLAY_SECTION_TRANSFORM] = "fixed:section:transform",
    [DISPLAY_SECTION_MODE] = "fixed:section:mode",
    [DISPLAY_SECTION_PROFILE] = "fixed:section:profile",
    [DISPLAY_SECTION_TOOL] = "fixed:section:tool",
};

char *anto_display_action_path(void) {
    const char *override = g_getenv("ANTO_MENU_ACTIONS_DIR");
    if (override && *override)
        return g_build_filename(override, "display.sh", NULL);
    return menu_home_path(".local/lib/anto-menu/actions/display.sh");
}

void anto_display_monitor_free(gpointer data) {
    DisplayMonitor *monitor = data;
    if (!monitor) return;
    g_free(monitor->name);
    g_free(monitor->description);
    g_free(monitor->resolution);
    g_free(monitor->refresh);
    g_free(monitor->scale);
    g_free(monitor->position);
    g_free(monitor->workspace);
    g_free(monitor->mirror);
    g_clear_pointer(&monitor->modes, g_ptr_array_unref);
    g_free(monitor);
}

void anto_display_mode_free(gpointer data) {
    DisplayMode *mode = data;
    if (!mode) return;
    g_free(mode->text);
    g_free(mode);
}

void anto_display_profile_free(gpointer data) {
    DisplayProfile *profile = data;
    if (!profile) return;
    g_free(profile->name);
    g_free(profile->summary);
    g_free(profile);
}

void anto_display_snapshot_free(gpointer data) {
    DisplaySnapshot *snapshot = data;
    if (!snapshot) return;
    g_clear_pointer(&snapshot->monitors, g_ptr_array_unref);
    g_clear_pointer(&snapshot->profiles, g_ptr_array_unref);
    g_clear_pointer(&snapshot->virtual_outputs, g_ptr_array_unref);
    g_free(snapshot->editor);
    g_free(snapshot->virtual_error);
    g_free(snapshot);
}

void anto_display_row_spec_free(gpointer data) {
    DisplayRowSpec *spec = data;
    if (!spec) return;
    g_free(spec->key);
    g_free(spec->icon);
    g_free(spec->title);
    g_free(spec->subtitle);
    g_free(spec->badge);
    g_free(spec->command);
    g_free(spec);
}

void anto_display_row_free(gpointer data) {
    DisplayRow *row = data;
    if (!row) return;
    g_free(row->key);
    g_free(row->icon_name);
    g_free(row->title_text);
    g_free(row->subtitle_text);
    g_free(row->badge_text);
    g_free(row->command);
    g_free(row->search_text);
    g_free(row);
}

void anto_display_view_free(gpointer data) {
    DisplayView *view = data;
    if (!view) return;
    if (view->runtime && view->runtime->view == view)
        view->runtime->view = NULL;
    if (view->loading && view->loading->row) {
        g_object_set_data(G_OBJECT(view->loading->row),
                          "menu-action", NULL);
        g_object_set_data(G_OBJECT(view->loading->row),
                          "display-key", NULL);
    }
    if (view->rows) {
        GHashTableIter iterator;
        gpointer value = NULL;
        g_hash_table_iter_init(&iterator, view->rows);
        while (g_hash_table_iter_next(&iterator, NULL, &value)) {
            DisplayRow *row = value;
            g_object_set_data(G_OBJECT(row->row), "menu-action", NULL);
            g_object_set_data(G_OBJECT(row->row), "display-key", NULL);
        }
    }
    anto_display_row_free(view->loading);
    g_clear_pointer(&view->rows, g_hash_table_unref);
    g_free(view->query);
    g_free(view);
}

gboolean anto_display_output_is_internal(const char *name) {
    return name && (g_str_has_prefix(name, "eDP") ||
                    g_str_has_prefix(name, "LVDS") ||
                    g_str_has_prefix(name, "DSI"));
}

gboolean anto_display_outputs_already_mirrored(GPtrArray *monitors) {
    const char *source = NULL;
    if (!monitors || monitors->len < 2) return FALSE;

    for (guint i = 0; i < monitors->len; i++) {
        DisplayMonitor *monitor = g_ptr_array_index(monitors, i);
        if (!monitor->enabled) return FALSE;
        if (!monitor->mirror || g_strcmp0(monitor->mirror, "none") == 0) {
            if (source) return FALSE;
            source = monitor->name;
        }
    }
    if (!source) return FALSE;

    for (guint i = 0; i < monitors->len; i++) {
        DisplayMonitor *monitor = g_ptr_array_index(monitors, i);
        if (g_strcmp0(monitor->name, source) == 0) continue;
        if (g_strcmp0(monitor->mirror, source) != 0) return FALSE;
    }
    return TRUE;
}

char *anto_display_command(const char *action, const char *first,
                             const char *second) {
    g_autofree char *path = anto_display_action_path();
    g_autofree char *quoted_path = g_shell_quote(path);
    g_autofree char *quoted_first = first ? g_shell_quote(first) : NULL;
    g_autofree char *quoted_second = second ? g_shell_quote(second) : NULL;
    if (first && second)
        return g_strdup_printf("%s %s %s %s", quoted_path, action,
                               quoted_first, quoted_second);
    if (first)
        return g_strdup_printf("%s %s %s", quoted_path, action, quoted_first);
    return g_strdup_printf("%s %s", quoted_path, action);
}

char *anto_display_virtual_backend_path(void) {
    const char *override = g_getenv("ANTO_MENU_VIRTUAL_BACKEND");
    if (override && *override) return g_strdup(override);
    g_autofree char *available = g_find_program_in_path("anto-menu");
    if (available) return g_strdup(available);
    return menu_home_path(".local/bin/anto-menu");
}

char *anto_display_virtual_command(const char *const arguments[]) {
    g_autofree char *path = anto_display_virtual_backend_path();
    g_autofree char *quoted_path = g_shell_quote(path);
    GString *command = g_string_new(quoted_path);
    for (guint index = 0; arguments[index]; index++) {
        g_autofree char *quoted = g_shell_quote(arguments[index]);
        g_string_append_c(command, ' ');
        g_string_append(command, quoted);
    }
    return g_string_free(command, FALSE);
}

DisplayRowSpec *anto_display_spec_add(
        GPtrArray *specs, DisplaySection section, const char *key,
        const char *icon, const char *title, const char *subtitle,
        const char *badge, const char *command, gboolean close_after) {
    DisplayRowSpec *spec = g_new0(DisplayRowSpec, 1);
    spec->key = g_strdup(key);
    spec->section = section;
    spec->icon = g_strdup(icon);
    spec->title = g_strdup(title);
    spec->subtitle = g_strdup(subtitle);
    spec->badge = g_strdup(badge);
    spec->command = g_strdup(command);
    spec->close_after = close_after;
    g_ptr_array_add(specs, spec);
    return spec;
}

void anto_display_virtual_command_finished(GObject *object,
                                             GAsyncResult *result,
                                             gpointer data) {
    DisplayRuntime *runtime = data;
    GSubprocess *process = G_SUBPROCESS(object);
    g_autofree char *stdout_text = NULL;
    g_autofree char *stderr_text = NULL;
    g_autoptr(GError) error = NULL;
    gboolean success = g_subprocess_communicate_utf8_finish(
        process, result, &stdout_text, &stderr_text, &error);
    success = success && g_subprocess_get_successful(process);

    GObject *window = g_weak_ref_get(&runtime->window);
    if (window) {
        if (!success) {
            const char *message =
                stderr_text && *g_strstrip(stderr_text)
                    ? stderr_text
                    : error && error->message
                          ? error->message
                          : "Operazione sul monitor virtuale non riuscita";
            menu_notify("Monitor virtuali", message);
        }
        if (g_strcmp0(runtime->app->current_page, "display") == 0)
            anto_display_refresh_start(runtime);
    }
    g_clear_object(&window);
    anto_display_runtime_unref(runtime);
}

void anto_display_arrange_monitor_free(gpointer data) {
    ArrangeMonitor *monitor = data;
    if (!monitor) return;
    g_free(monitor->name);
    g_free(monitor->description);
    g_free(monitor->resolution);
    g_free(monitor);
}

void anto_display_arranger_free(gpointer data) {
    DisplayArranger *arranger = data;
    if (!arranger) return;
    if (anto_display_active_arranger == arranger) anto_display_active_arranger = NULL;
    g_clear_pointer(&arranger->monitors, g_ptr_array_unref);
    g_free(arranger);
}

gboolean anto_display_monitor_logical_geometry(const DisplayMonitor *monitor,
                                         double *x, double *y,
                                         double *width, double *height) {
    int pixel_width = 0;
    int pixel_height = 0;
    int position_x = 0;
    int position_y = 0;
    double scale = g_ascii_strtod(monitor->scale, NULL);
    if (!monitor->enabled || scale <= 0.0 ||
        sscanf(monitor->resolution, "%dx%d", &pixel_width, &pixel_height) != 2 ||
        sscanf(monitor->position, "%d,%d", &position_x, &position_y) != 2 ||
        pixel_width <= 0 || pixel_height <= 0)
        return FALSE;

    if ((monitor->transform & 1) != 0) {
        int swap = pixel_width;
        pixel_width = pixel_height;
        pixel_height = swap;
    }
    *x = position_x;
    *y = position_y;
    *width = pixel_width / scale;
    *height = pixel_height / scale;
    return isfinite(*width) && isfinite(*height) && *width > 0.0 && *height > 0.0;
}

void anto_display_rounded_rectangle(cairo_t *cr, double x, double y,
                              double width, double height, double radius) {
    double right = x + width;
    double bottom = y + height;
    radius = MIN(radius, MIN(width, height) / 2.0);
    cairo_new_sub_path(cr);
    cairo_arc(cr, right - radius, y + radius, radius, -G_PI_2, 0);
    cairo_arc(cr, right - radius, bottom - radius, radius, 0, G_PI_2);
    cairo_arc(cr, x + radius, bottom - radius, radius, G_PI_2, G_PI);
    cairo_arc(cr, x + radius, y + radius, radius, G_PI, 3.0 * G_PI_2);
    cairo_close_path(cr);
}

void anto_display_set_source_rgba(cairo_t *cr, const GdkRGBA *color, double alpha) {
    cairo_set_source_rgba(cr, color->red, color->green, color->blue,
                          color->alpha * alpha);
}

void anto_display_draw_centered_text(cairo_t *cr, const char *text,
                               const char *font, const GdkRGBA *color,
                               double alpha, double x, double y, double width) {
    PangoLayout *layout = pango_cairo_create_layout(cr);
    PangoFontDescription *description = pango_font_description_from_string(font);
    pango_layout_set_font_description(layout, description);
    pango_layout_set_text(layout, text ? text : "", -1);
    pango_layout_set_width(layout, (int)(MAX(width, 1.0) * PANGO_SCALE));
    pango_layout_set_alignment(layout, PANGO_ALIGN_CENTER);
    pango_layout_set_ellipsize(layout, PANGO_ELLIPSIZE_END);
    int text_width = 0;
    int text_height = 0;
    pango_layout_get_pixel_size(layout, &text_width, &text_height);
    (void)text_width;
    anto_display_set_source_rgba(cr, color, alpha);
    cairo_move_to(cr, x, y - text_height / 2.0);
    pango_cairo_show_layout(cr, layout);
    pango_font_description_free(description);
    g_object_unref(layout);
}

gboolean anto_display_rectangles_overlap(const ArrangeMonitor *left,
                                    const ArrangeMonitor *right) {
    const double epsilon = 0.5;
    return left->x < right->x + right->width - epsilon &&
           left->x + left->width > right->x + epsilon &&
           left->y < right->y + right->height - epsilon &&
           left->y + left->height > right->y + epsilon;
}

gboolean anto_display_monitor_overlaps_any(const DisplayArranger *arranger,
                                     guint index) {
    ArrangeMonitor *monitor = g_ptr_array_index(arranger->monitors, index);
    for (guint i = 0; i < arranger->monitors->len; i++) {
        if (i == index) continue;
        if (anto_display_rectangles_overlap(monitor, g_ptr_array_index(arranger->monitors, i)))
            return TRUE;
    }
    return FALSE;
}

void anto_display_arranger_draw(GtkDrawingArea *area, cairo_t *cr,
                          int width, int height, gpointer data) {
    DisplayArranger *arranger = data;
    GdkRGBA accent = {0.48, 0.64, 0.97, 1.0};
    GdkRGBA foreground = {0.82, 0.84, 0.92, 1.0};
    GdkRGBA muted = {0.58, 0.62, 0.72, 1.0};
    gtk_widget_get_color(GTK_WIDGET(area), &accent);
    gtk_widget_get_color(arranger->title_label, &foreground);
    gtk_widget_get_color(arranger->selection_label, &muted);
    GdkRGBA select = {
        accent.red * 0.68 + foreground.red * 0.32,
        accent.green * 0.68 + foreground.green * 0.32,
        accent.blue * 0.68 + foreground.blue * 0.32,
        1.0,
    };
    GdkRGBA red = {
        MAX(accent.red, 0.78), accent.green * 0.42,
        accent.blue * 0.48, 1.0,
    };

    anto_display_rounded_rectangle(cr, 1.0, 1.0, width - 2.0, height - 2.0, 19.0);
    cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.24);
    cairo_fill_preserve(cr);
    anto_display_set_source_rgba(cr, &foreground, 0.09);
    cairo_set_line_width(cr, 1.0);
    cairo_stroke(cr);

    cairo_save(cr);
    anto_display_rounded_rectangle(cr, 2.0, 2.0, width - 4.0, height - 4.0, 18.0);
    cairo_clip(cr);
    anto_display_set_source_rgba(cr, &foreground, 0.035);
    cairo_set_line_width(cr, 1.0);
    for (int x = 22; x < width; x += 28) {
        cairo_move_to(cr, x + 0.5, 0);
        cairo_line_to(cr, x + 0.5, height);
    }
    for (int y = 22; y < height; y += 28) {
        cairo_move_to(cr, 0, y + 0.5);
        cairo_line_to(cr, width, y + 0.5);
    }
    cairo_stroke(cr);
    cairo_restore(cr);

    if (!arranger->drag_active || arranger->view_scale <= 0.0)
        anto_display_arranger_compute_view(arranger, width, height);
    for (guint i = 0; i < arranger->monitors->len; i++) {
        ArrangeMonitor *monitor = g_ptr_array_index(arranger->monitors, i);
        double x = 0.0;
        double y = 0.0;
        double monitor_width = 0.0;
        double monitor_height = 0.0;
        anto_display_arranger_view_rect(arranger, monitor, &x, &y, &monitor_width, &monitor_height);
        gboolean selected = (int)i == arranger->selected;
        gboolean overlapping = anto_display_monitor_overlaps_any(arranger, i);
        GdkRGBA *edge = overlapping ? &red : selected ? &accent : &foreground;

        anto_display_rounded_rectangle(cr, x + 3.0, y + 7.0, monitor_width, monitor_height, 15.0);
        cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.24);
        cairo_fill(cr);

        anto_display_rounded_rectangle(cr, x, y, monitor_width, monitor_height, 15.0);
        anto_display_set_source_rgba(cr, selected ? &accent : &foreground,
                        selected ? 0.27 : 0.105);
        cairo_fill_preserve(cr);
        anto_display_set_source_rgba(cr, edge, overlapping ? 0.95 : selected ? 0.92 : 0.28);
        cairo_set_line_width(cr, selected ? 2.5 : 1.2);
        cairo_stroke(cr);

        anto_display_rounded_rectangle(cr, x + 1.0, y + 1.0, monitor_width - 2.0,
                          MIN(34.0, monitor_height * 0.24), 13.0);
        anto_display_set_source_rgba(cr, selected ? &accent : &select, selected ? 0.22 : 0.09);
        cairo_fill(cr);

        g_autofree char *number = g_strdup_printf("%u", i + 1);
        anto_display_draw_centered_text(cr, number, "JetBrainsMono Nerd Font Bold 9",
                           &accent, 0.9, x + 7.0, y + 17.0, 20.0);
        if (monitor->focused) {
            cairo_arc(cr, x + monitor_width - 14.0, y + 14.0, 4.0, 0, 2.0 * G_PI);
            anto_display_set_source_rgba(cr, &accent, 1.0);
            cairo_fill(cr);
        }

        const char *name_font = monitor_width < 135.0
                                    ? "JetBrainsMono Nerd Font Bold 8"
                                    : "JetBrainsMono Nerd Font Bold 11";
        const char *detail_font = monitor_width < 135.0
                                      ? "JetBrainsMono Nerd Font 7"
                                      : "JetBrainsMono Nerd Font 9";
        anto_display_draw_centered_text(cr, monitor->name, name_font, &foreground, 1.0,
                           x + 9.0, y + monitor_height * 0.39,
                           monitor_width - 18.0);
        anto_display_draw_centered_text(cr, monitor->resolution, detail_font, &accent, 0.95,
                           x + 9.0, y + monitor_height * 0.56,
                           monitor_width - 18.0);
        if (monitor_height > 92.0)
            anto_display_draw_centered_text(cr, monitor->description,
                               "JetBrainsMono Nerd Font 7", &muted, 0.82,
                               x + 9.0, y + monitor_height * 0.70,
                               monitor_width - 18.0);

        g_autofree char *position = g_strdup_printf(
            "%.0f,%.0f  ·  %.0f×%.0f", monitor->x, monitor->y,
            monitor->width, monitor->height);
        anto_display_draw_centered_text(cr, position, "JetBrainsMono Nerd Font 7",
                           overlapping ? &red : &muted, 0.78,
                           x + 8.0, y + monitor_height - 13.0,
                           monitor_width - 16.0);
    }
}

int anto_display_arranger_hit_test(DisplayArranger *arranger, double pointer_x,
                             double pointer_y) {
    int canvas_width = gtk_widget_get_width(arranger->area);
    int canvas_height = gtk_widget_get_height(arranger->area);
    anto_display_arranger_compute_view(arranger, canvas_width, canvas_height);
    for (int i = (int)arranger->monitors->len - 1; i >= 0; i--) {
        ArrangeMonitor *monitor = g_ptr_array_index(arranger->monitors, (guint)i);
        double x = 0.0;
        double y = 0.0;
        double width = 0.0;
        double height = 0.0;
        anto_display_arranger_view_rect(arranger, monitor, &x, &y, &width, &height);
        if (pointer_x >= x && pointer_x <= x + width &&
            pointer_y >= y && pointer_y <= y + height)
            return i;
    }
    return -1;
}

gboolean anto_display_arranger_has_changes(const DisplayArranger *arranger) {
    for (guint i = 0; i < arranger->monitors->len; i++) {
        ArrangeMonitor *monitor = g_ptr_array_index(arranger->monitors, i);
        if (fabs(monitor->x - monitor->initial_x) >= 0.5 ||
            fabs(monitor->y - monitor->initial_y) >= 0.5)
            return TRUE;
    }
    return FALSE;
}

void anto_display_arranger_update_controls(DisplayArranger *arranger) {
    gboolean changed = anto_display_arranger_has_changes(arranger);
    gtk_widget_set_sensitive(arranger->apply_button, changed);
    gtk_widget_set_sensitive(arranger->reset_button, changed);
    if (arranger->selected >= 0 &&
        arranger->selected < (int)arranger->monitors->len) {
        ArrangeMonitor *monitor = g_ptr_array_index(
            arranger->monitors, (guint)arranger->selected);
        g_autofree char *text = g_strdup_printf(
            "%s selezionato · %s · %.0f×%.0f logici · posizione %.0f,%.0f%s",
            monitor->name, monitor->resolution, monitor->width, monitor->height,
            monitor->x, monitor->y,
            changed ? " · anteprima modificata" : "");
        gtk_label_set_text(GTK_LABEL(arranger->selection_label), text);
    } else {
        gtk_label_set_text(GTK_LABEL(arranger->selection_label),
                           "Seleziona un rettangolo e trascinalo accanto agli altri");
    }
}

gboolean anto_display_live_refresh_idle(gpointer data) {
    menu_display_live_event(data);
    return G_SOURCE_REMOVE;
}

void anto_display_arranger_flush_deferred_refresh(DisplayArranger *arranger) {
    if (!arranger->refresh_pending || arranger->drag_active ||
        arranger->applying || anto_display_arranger_has_changes(arranger))
        return;
    arranger->refresh_pending = FALSE;
    g_idle_add(anto_display_live_refresh_idle, arranger->app);
}

gboolean anto_display_ranges_near(double first_start, double first_end,
                            double second_start, double second_end,
                            double tolerance) {
    return first_end >= second_start - tolerance &&
           second_end >= first_start - tolerance;
}

void anto_display_consider_snap(double delta, double threshold,
                          double *best_delta, gboolean *found) {
    if (fabs(delta) <= threshold && (!*found || fabs(delta) < fabs(*best_delta))) {
        *best_delta = delta;
        *found = TRUE;
    }
}

void anto_display_arranger_snap_monitor(DisplayArranger *arranger, guint selected) {
    ArrangeMonitor *monitor = g_ptr_array_index(arranger->monitors, selected);
    const double threshold = 72.0;
    double best_x = 0.0;
    double best_y = 0.0;
    gboolean snap_x = FALSE;
    gboolean snap_y = FALSE;

    for (guint i = 0; i < arranger->monitors->len; i++) {
        if (i == selected) continue;
        ArrangeMonitor *other = g_ptr_array_index(arranger->monitors, i);
        if (anto_display_ranges_near(monitor->y, monitor->y + monitor->height,
                        other->y, other->y + other->height, threshold)) {
            anto_display_consider_snap(other->x - (monitor->x + monitor->width), threshold,
                          &best_x, &snap_x);
            anto_display_consider_snap(other->x + other->width - monitor->x, threshold,
                          &best_x, &snap_x);
            anto_display_consider_snap(other->x - monitor->x, threshold, &best_x, &snap_x);
            anto_display_consider_snap(other->x + other->width -
                              (monitor->x + monitor->width),
                          threshold, &best_x, &snap_x);
        }
        if (anto_display_ranges_near(monitor->x, monitor->x + monitor->width,
                        other->x, other->x + other->width, threshold)) {
            anto_display_consider_snap(other->y - (monitor->y + monitor->height), threshold,
                          &best_y, &snap_y);
            anto_display_consider_snap(other->y + other->height - monitor->y, threshold,
                          &best_y, &snap_y);
            anto_display_consider_snap(other->y - monitor->y, threshold, &best_y, &snap_y);
            anto_display_consider_snap(other->y + other->height -
                              (monitor->y + monitor->height),
                          threshold, &best_y, &snap_y);
        }
    }
    if (snap_x) monitor->x += best_x;
    if (snap_y) monitor->y += best_y;
}

void anto_display_arranger_resolve_overlaps(DisplayArranger *arranger, guint selected) {
    ArrangeMonitor *monitor = g_ptr_array_index(arranger->monitors, selected);
    guint max_passes = MAX(arranger->monitors->len * 4, 4u);
    for (guint pass = 0; pass < max_passes; pass++) {
        gboolean found = FALSE;
        for (guint i = 0; i < arranger->monitors->len; i++) {
            if (i == selected) continue;
            ArrangeMonitor *other = g_ptr_array_index(arranger->monitors, i);
            if (!anto_display_rectangles_overlap(monitor, other)) continue;
            found = TRUE;
            double displacements[4] = {
                other->x - (monitor->x + monitor->width),
                other->x + other->width - monitor->x,
                other->y - (monitor->y + monitor->height),
                other->y + other->height - monitor->y,
            };
            guint best = 0;
            for (guint candidate = 1; candidate < G_N_ELEMENTS(displacements);
                 candidate++)
                if (fabs(displacements[candidate]) < fabs(displacements[best]))
                    best = candidate;
            if (best < 2) monitor->x += displacements[best];
            else monitor->y += displacements[best];
        }
        if (!found) break;
    }
}

void anto_display_arranger_normalize(DisplayArranger *arranger) {
    double min_x = G_MAXDOUBLE;
    double min_y = G_MAXDOUBLE;
    for (guint i = 0; i < arranger->monitors->len; i++) {
        ArrangeMonitor *monitor = g_ptr_array_index(arranger->monitors, i);
        min_x = MIN(min_x, monitor->x);
        min_y = MIN(min_y, monitor->y);
    }
    if (arranger->monitors->len == 0) return;
    for (guint i = 0; i < arranger->monitors->len; i++) {
        ArrangeMonitor *monitor = g_ptr_array_index(arranger->monitors, i);
        monitor->x = round(monitor->x - min_x);
        monitor->y = round(monitor->y - min_y);
    }
}

void anto_display_arranger_drag_begin(GtkGestureDrag *gesture, double start_x,
                                double start_y, gpointer data) {
    DisplayArranger *arranger = data;
    arranger->selected = anto_display_arranger_hit_test(arranger, start_x, start_y);
    arranger->dragged = FALSE;
    if (arranger->selected < 0) {
        gtk_gesture_set_state(GTK_GESTURE(gesture), GTK_EVENT_SEQUENCE_DENIED);
        anto_display_arranger_update_controls(arranger);
        gtk_widget_queue_draw(arranger->area);
        return;
    }
    ArrangeMonitor *monitor = g_ptr_array_index(
        arranger->monitors, (guint)arranger->selected);
    arranger->drag_active = TRUE;
    arranger->drag_origin_x = monitor->x;
    arranger->drag_origin_y = monitor->y;
    gtk_widget_set_cursor_from_name(arranger->area, "grabbing");
    anto_display_arranger_update_controls(arranger);
    gtk_widget_queue_draw(arranger->area);
}

void anto_display_arranger_drag_update(GtkGestureDrag *gesture, double offset_x,
                                 double offset_y, gpointer data) {
    (void)gesture;
    DisplayArranger *arranger = data;
    if (arranger->selected < 0 || arranger->view_scale <= 0.0) return;
    if (hypot(offset_x, offset_y) < 6.0 && !arranger->dragged) return;
    arranger->dragged = TRUE;
    ArrangeMonitor *monitor = g_ptr_array_index(
        arranger->monitors, (guint)arranger->selected);
    monitor->x = arranger->drag_origin_x + offset_x / arranger->view_scale;
    monitor->y = arranger->drag_origin_y + offset_y / arranger->view_scale;
    anto_display_arranger_update_controls(arranger);
    gtk_widget_queue_draw(arranger->area);
}

void anto_display_arranger_drag_end(GtkGestureDrag *gesture, double offset_x,
                              double offset_y, gpointer data) {
    (void)gesture;
    DisplayArranger *arranger = data;
    gtk_widget_set_cursor_from_name(arranger->area, "grab");
    if (arranger->selected < 0) return;
    if (!arranger->dragged && hypot(offset_x, offset_y) >= 6.0)
        anto_display_arranger_drag_update(gesture, offset_x, offset_y, data);
    if (arranger->dragged) {
        guint selected = (guint)arranger->selected;
        anto_display_arranger_snap_monitor(arranger, selected);
        anto_display_arranger_resolve_overlaps(arranger, selected);
        anto_display_arranger_normalize(arranger);
    }
    arranger->drag_active = FALSE;
    anto_display_arranger_update_controls(arranger);
    gtk_widget_queue_draw(arranger->area);
    anto_display_arranger_flush_deferred_refresh(arranger);
}

void anto_display_arranger_sync_monitors(DisplayArranger *arranger,
                                   GPtrArray *monitors) {
    g_autofree char *selected_name = NULL;
    if (arranger->selected >= 0 &&
        arranger->selected < (int)arranger->monitors->len) {
        ArrangeMonitor *selected = g_ptr_array_index(
            arranger->monitors, (guint)arranger->selected);
        selected_name = g_strdup(selected->name);
    }

    GPtrArray *previous = arranger->monitors;
    GPtrArray *updated =
        g_ptr_array_new_with_free_func(anto_display_arrange_monitor_free);
    guint mirrored_count = 0;
    int selected_index = -1;
    int focused_index = -1;

    for (guint i = 0; i < monitors->len; i++) {
        DisplayMonitor *source = g_ptr_array_index(monitors, i);
        if (!source->enabled) continue;
        if (source->mirror && g_strcmp0(source->mirror, "none") != 0) {
            mirrored_count++;
            continue;
        }

        double x = 0.0;
        double y = 0.0;
        double width = 0.0;
        double height = 0.0;
        if (!anto_display_monitor_logical_geometry(source, &x, &y, &width, &height))
            continue;

        ArrangeMonitor *monitor = NULL;
        for (guint old_index = 0; old_index < previous->len; old_index++) {
            ArrangeMonitor *candidate =
                g_ptr_array_index(previous, old_index);
            if (g_strcmp0(candidate->name, source->name) == 0) {
                monitor = g_ptr_array_steal_index(previous, old_index);
                break;
            }
        }
        if (!monitor) monitor = g_new0(ArrangeMonitor, 1);
        g_free(monitor->name);
        g_free(monitor->description);
        g_free(monitor->resolution);
        monitor->name = g_strdup(source->name);
        monitor->description = g_strdup(source->description);
        monitor->resolution = g_strdup(source->resolution);
        monitor->x = monitor->initial_x = x;
        monitor->y = monitor->initial_y = y;
        monitor->width = width;
        monitor->height = height;
        monitor->focused = source->focused;
        monitor->internal = anto_display_output_is_internal(source->name);
        int new_index = (int)updated->len;
        if (selected_name &&
            g_strcmp0(selected_name, source->name) == 0)
            selected_index = new_index;
        if (source->focused) focused_index = new_index;
        g_ptr_array_add(updated, monitor);
    }

    g_ptr_array_unref(previous);
    arranger->monitors = updated;
    arranger->selected = selected_index >= 0 ? selected_index :
                         focused_index >= 0 ? focused_index :
                         updated->len > 0 ? 0 : -1;
    g_autofree char *description = mirrored_count > 0
        ? g_strdup_printf(
              "Trascina le uscite estese · %u duplicati seguono la sorgente · il pulsante salva al riavvio",
              mirrored_count)
        : g_strdup(
              "Trascina e allinea · Applica e salva mantiene il layout al riavvio");
    gtk_label_set_text(GTK_LABEL(arranger->detail_label), description);
    anto_display_arranger_update_controls(arranger);
    gtk_widget_queue_draw(arranger->area);
}

GPtrArray *anto_display_parse_monitors(const char *output) {
    GPtrArray *monitors = g_ptr_array_new_with_free_func(anto_display_monitor_free);
    g_auto(GStrv) lines = g_strsplit(output ? output : "", "\n", -1);
    for (guint i = 0; lines[i]; i++) {
        if (!*lines[i]) continue;
        g_auto(GStrv) fields = g_strsplit(lines[i], "\t", -1);
        if (g_strv_length(fields) < 13 || !fields[0] || !*fields[0]) continue;
        DisplayMonitor *monitor = g_new0(DisplayMonitor, 1);
        monitor->name = g_strdup(fields[0]);
        monitor->description = g_strdup(fields[1]);
        monitor->enabled = g_strcmp0(fields[2], "on") == 0;
        monitor->resolution = g_strdup(fields[3]);
        monitor->refresh = g_strdup(fields[4]);
        monitor->scale = g_strdup(fields[5]);
        monitor->transform = (int)g_ascii_strtoll(fields[6], NULL, 10);
        monitor->position = g_strdup(fields[7]);
        monitor->workspace = g_strdup(fields[8]);
        monitor->focused = g_strcmp0(fields[9], "true") == 0;
        monitor->mirror = g_strdup(fields[10]);
        monitor->dpms = g_strcmp0(fields[11], "true") == 0;
        monitor->mode_count = (int)g_ascii_strtoll(fields[12], NULL, 10);
        monitor->modes = g_ptr_array_new_with_free_func(anto_display_mode_free);
        g_ptr_array_add(monitors, monitor);
    }
    return monitors;
}

GPtrArray *anto_display_parse_modes(const char *output) {
    GPtrArray *modes = g_ptr_array_new_with_free_func(anto_display_mode_free);
    g_auto(GStrv) lines = g_strsplit(output ? output : "", "\n", -1);
    for (guint i = 0; lines[i]; i++) {
        int width = 0, height = 0;
        double rate = 0;
        if (sscanf(lines[i], "%dx%d@%lf", &width, &height, &rate) != 3) continue;
        if (width <= 0 || height <= 0 || rate <= 0) continue;
        DisplayMode *mode = g_new0(DisplayMode, 1);
        mode->text = g_strdup(lines[i]);
        mode->width = width;
        mode->height = height;
        mode->rate = rate;
        g_ptr_array_add(modes, mode);
    }
    return modes;
}

GPtrArray *anto_display_parse_profiles(const char *output) {
    GPtrArray *profiles = g_ptr_array_new_with_free_func(anto_display_profile_free);
    g_auto(GStrv) lines = g_strsplit(output ? output : "", "\n", -1);
    for (guint i = 0; lines[i]; i++) {
        if (!*lines[i]) continue;
        g_auto(GStrv) fields = g_strsplit(lines[i], "\t", 2);
        if (!fields[0] || !*fields[0]) continue;
        DisplayProfile *profile = g_new0(DisplayProfile, 1);
        profile->name = g_strdup(fields[0]);
        profile->summary = g_strdup(
            fields[1] && *fields[1] ? fields[1] : "Configurazione salvata");
        g_ptr_array_add(profiles, profile);
    }
    return profiles;
}

gboolean anto_display_read_display_command(const char *script, const char *action,
                                     const char *argument,
                                     GCancellable *cancellable,
                                     char **output, GError **error) {
    const char *with_argument[] = {
        "/usr/bin/timeout", "--foreground", "--kill-after=1", "3",
        script, action, argument, NULL
    };
    const char *without_argument[] = {
        "/usr/bin/timeout", "--foreground", "--kill-after=1", "3",
        script, action, NULL
    };
    const char *const *argv = argument ? with_argument : without_argument;
    g_autoptr(GSubprocess) process = g_subprocess_newv(
        argv, G_SUBPROCESS_FLAGS_STDOUT_PIPE | G_SUBPROCESS_FLAGS_STDERR_PIPE,
        error);
    if (!process) return FALSE;

    char *stdout_text = NULL;
    char *stderr_text = NULL;
    gboolean communicated = g_subprocess_communicate_utf8(
        process, NULL, cancellable, &stdout_text, &stderr_text, error);
    if (!communicated) {
        g_subprocess_force_exit(process);
        g_free(stdout_text);
        g_free(stderr_text);
        return FALSE;
    }
    if (!g_subprocess_get_successful(process)) {
        const char *message = stderr_text && *g_strstrip(stderr_text)
                                  ? stderr_text
                                  : "lettura display scaduta o non riuscita";
        g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_FAILED, message);
        g_free(stdout_text);
        g_free(stderr_text);
        return FALSE;
    }
    g_free(stderr_text);
    *output = stdout_text ? stdout_text : g_strdup("");
    return TRUE;
}

void anto_display_snapshot_load(GTask *task, gpointer source,
                                  gpointer task_data,
                                  GCancellable *cancellable) {
    (void)source;
    const char *script = task_data;
    g_autoptr(GError) error = NULL;
    g_autofree char *output = NULL;
    DisplaySnapshot *snapshot = g_new0(DisplaySnapshot, 1);

    if (!anto_display_read_display_command(script, "list", NULL, cancellable,
                              &output, &error)) {
        anto_display_snapshot_free(snapshot);
        g_task_return_error(task, g_steal_pointer(&error));
        return;
    }
    snapshot->monitors = anto_display_parse_monitors(output);
    g_clear_pointer(&output, g_free);

    g_clear_error(&error);
    snapshot->virtual_outputs = menu_virtual_output_load(&error);
    if (!snapshot->virtual_outputs) {
        snapshot->partial = TRUE;
        snapshot->virtual_error = g_strdup(
            error && error->message
                ? error->message
                : "Stato dei monitor virtuali non disponibile");
        snapshot->virtual_outputs =
            g_ptr_array_new_with_free_func(menu_virtual_output_free);
        g_clear_error(&error);
    }

    for (guint i = 0; i < snapshot->monitors->len; i++) {
        DisplayMonitor *monitor = g_ptr_array_index(snapshot->monitors, i);
        if (cancellable && g_cancellable_is_cancelled(cancellable)) {
            anto_display_snapshot_free(snapshot);
            g_task_return_new_error(task, G_IO_ERROR, G_IO_ERROR_CANCELLED,
                                    "Aggiornamento schermi annullato");
            return;
        }
        if (!monitor->enabled) continue;
        g_clear_error(&error);
        if (anto_display_read_display_command(script, "modes", monitor->name, cancellable,
                                 &output, &error)) {
            g_clear_pointer(&monitor->modes, g_ptr_array_unref);
            monitor->modes = anto_display_parse_modes(output);
            g_clear_pointer(&output, g_free);
        } else {
            snapshot->partial = TRUE;
        }
    }

    g_clear_error(&error);
    if (anto_display_read_display_command(script, "profile-list", NULL, cancellable,
                             &output, &error)) {
        snapshot->profiles = anto_display_parse_profiles(output);
        g_clear_pointer(&output, g_free);
    } else {
        snapshot->partial = TRUE;
        snapshot->profiles = g_ptr_array_new_with_free_func(anto_display_profile_free);
    }

    g_autofree char *last_profile =
        anto_local_config_path("display/profiles/last.json", NULL);
    snapshot->has_last = g_file_test(last_profile, G_FILE_TEST_IS_REGULAR);

    g_autofree char *nwg_displays = g_find_program_in_path("nwg-displays");
    g_autofree char *wdisplays = g_find_program_in_path("wdisplays");
    if (nwg_displays) snapshot->editor = g_strdup("nwg-displays");
    else if (wdisplays) snapshot->editor = g_strdup("wdisplays");

    g_task_return_pointer(task, snapshot, anto_display_snapshot_free);
}

DisplayRuntime *anto_display_runtime_ref(DisplayRuntime *runtime) {
    g_atomic_int_inc(&runtime->refs);
    return runtime;
}

void anto_display_runtime_unref(gpointer data) {
    DisplayRuntime *runtime = data;
    if (!runtime || !g_atomic_int_dec_and_test(&runtime->refs)) return;
    if (runtime->view) {
        runtime->view->runtime = NULL;
        runtime->view = NULL;
    }
    g_weak_ref_clear(&runtime->window);
    anto_display_snapshot_free(runtime->snapshot);
    g_free(runtime);
}

DisplayRuntime *anto_display_runtime_get(MenuApp *app) {
    if (!app || !app->window) return NULL;
    DisplayRuntime *runtime =
        g_object_get_data(G_OBJECT(app->window), DISPLAY_RUNTIME_KEY);
    if (runtime) return runtime;
    runtime = g_new0(DisplayRuntime, 1);
    runtime->refs = 1;
    runtime->app = app;
    g_weak_ref_init(&runtime->window, G_OBJECT(app->window));
    g_object_set_data_full(G_OBJECT(app->window), DISPLAY_RUNTIME_KEY,
                           runtime, anto_display_runtime_unref);
    return runtime;
}

const char *anto_display_transform_name(int transform) {
    switch (transform) {
        case 0: return "normale";
        case 1: return "90°";
        case 2: return "180°";
        case 3: return "270°";
        case 4: return "ribaltato";
        case 5: return "ribaltato + 90°";
        case 6: return "ribaltato + 180°";
        case 7: return "ribaltato + 270°";
        default: return "sconosciuto";
    }
}

char *anto_display_monitor_subtitle(const DisplayMonitor *monitor) {
    if (!monitor->enabled)
        return g_strdup_printf("%s · collegato, uscita disattivata · %d modalità",
                               monitor->name, monitor->mode_count);
    double scale = g_ascii_strtod(monitor->scale, NULL) * 100.0;
    double refresh = g_ascii_strtod(monitor->refresh, NULL);
    const char *mirror = monitor->mirror && g_strcmp0(monitor->mirror, "none") != 0
                             ? monitor->mirror
                             : NULL;
    return g_strdup_printf(
        "%s · %s @ %.2f Hz · scala %.0f%%\n%s · posizione %s · workspace %s%s%s%s",
        monitor->name, monitor->resolution, refresh, scale,
        anto_display_transform_name(monitor->transform), monitor->position, monitor->workspace,
        mirror ? " · duplica " : "", mirror ? mirror : "",
        monitor->dpms ? "" : " · DPMS spento");
}

gboolean anto_display_same_mode(const DisplayMode *left, const DisplayMode *right) {
    return left && right && g_strcmp0(left->text, right->text) == 0;
}

void anto_display_add_mode_once(GPtrArray *chosen, DisplayMode *mode) {
    if (!mode) return;
    for (guint i = 0; i < chosen->len; i++)
        if (anto_display_same_mode(g_ptr_array_index(chosen, i), mode)) return;
    g_ptr_array_add(chosen, mode);
}

GPtrArray *anto_display_recommended_modes(GPtrArray *modes,
                                    const DisplayMonitor *monitor) {
    GPtrArray *chosen = g_ptr_array_new();
    DisplayMode *current = NULL;
    DisplayMode *max_pixels = NULL;
    DisplayMode *max_rate = NULL;
    DisplayMode *current_sixty = NULL;
    int current_width = 0, current_height = 0;
    (void)sscanf(monitor->resolution, "%dx%d", &current_width, &current_height);

    for (guint i = 0; i < modes->len; i++) {
        DisplayMode *mode = g_ptr_array_index(modes, i);
        long long pixels = (long long)mode->width * mode->height;
        long long best_pixels = max_pixels
                                    ? (long long)max_pixels->width * max_pixels->height
                                    : -1;
        if (!max_pixels || pixels > best_pixels ||
            (pixels == best_pixels && mode->rate > max_pixels->rate))
            max_pixels = mode;
        if (!max_rate || mode->rate > max_rate->rate ||
            (fabs(mode->rate - max_rate->rate) < 0.02 &&
             pixels > (long long)max_rate->width * max_rate->height))
            max_rate = mode;
        if (mode->width == current_width && mode->height == current_height) {
            double current_refresh = g_ascii_strtod(monitor->refresh, NULL);
            if (!current || fabs(mode->rate - current_refresh) <
                                fabs(current->rate - current_refresh))
                current = mode;
            if (fabs(mode->rate - 60.0) < 0.25 &&
                (!current_sixty || mode->rate > current_sixty->rate))
                current_sixty = mode;
        }
    }

    anto_display_add_mode_once(chosen, current);
    anto_display_add_mode_once(chosen, max_pixels);
    anto_display_add_mode_once(chosen, max_rate);
    anto_display_add_mode_once(chosen, current_sixty);
    return chosen;
}

DisplayMonitor *anto_display_monitor_named(GPtrArray *monitors,
                                             const char *name) {
    for (guint index = 0; monitors && index < monitors->len; index++) {
        DisplayMonitor *monitor = g_ptr_array_index(monitors, index);
        if (g_strcmp0(monitor->name, name) == 0) return monitor;
    }
    return NULL;
}

gboolean anto_display_monitor_is_managed_virtual(
        const DisplaySnapshot *snapshot, const DisplayMonitor *monitor) {
    for (guint index = 0;
         snapshot->virtual_outputs &&
         index < snapshot->virtual_outputs->len;
         index++) {
        MenuVirtualOutput *output =
            g_ptr_array_index(snapshot->virtual_outputs, index);
        if (g_strcmp0(output->name, monitor->name) == 0) return TRUE;
    }
    return FALSE;
}

void anto_display_collect_virtual_specs(GPtrArray *specs,
                                  const DisplaySnapshot *snapshot) {
    static const struct {
        const char *key;
        const char *title;
        const char *width;
        const char *height;
        const char *icon;
    } presets[] = {
        {
            "1920x1080", "Desktop · 1920×1080", "1920", "1080",
            "video-display-symbolic",
        },
        {
            "1080x1920", "Verticale · 1080×1920", "1080", "1920",
            "object-rotate-right-symbolic",
        },
        {
            "2560x1440", "QHD · 2560×1440", "2560", "1440",
            "video-display-symbolic",
        },
    };
    gboolean has_offline_persistent = FALSE;

    if (snapshot->virtual_error) {
        anto_display_spec_add(
            specs, DISPLAY_SECTION_VIRTUAL, "virtual:state:error",
            "dialog-warning-symbolic", "Stato virtuale non leggibile",
            snapshot->virtual_error, "ERRORE", NULL, FALSE);
    } else if (!snapshot->virtual_outputs ||
               snapshot->virtual_outputs->len == 0) {
        anto_display_spec_add(
            specs, DISPLAY_SECTION_VIRTUAL, "virtual:state:empty",
            "computer-symbolic", "Nessun monitor virtuale gestito",
            "I preset creano output headless isolati e salvati localmente",
            "PRONTO", NULL, FALSE);
    }

    for (guint index = 0;
         snapshot->virtual_outputs &&
         index < snapshot->virtual_outputs->len;
         index++) {
        MenuVirtualOutput *output =
            g_ptr_array_index(snapshot->virtual_outputs, index);
        DisplayMonitor *live =
            anto_display_monitor_named(snapshot->monitors, output->name);
        gboolean online = live != NULL;
        if (!online && output->persistent)
            has_offline_persistent = TRUE;

        g_autofree char *key = g_strdup_printf(
            "virtual:output:%s:summary", output->name);
        g_autofree char *subtitle = g_strdup_printf(
            "%d×%d @ %.0f Hz · scala %.0f%% · posizione %d,%d\n"
            "%s · configurazione %s",
            output->width, output->height, output->refresh,
            output->scale * 100.0, output->x, output->y,
            online ? "Output presente in Hyprland" : "Output non attivo",
            output->persistent ? "ripristinata all’avvio"
                               : "valida per questa sessione");
        anto_display_spec_add(
            specs, DISPLAY_SECTION_VIRTUAL, key,
            online ? "network-transmit-receive-symbolic"
                   : "network-offline-symbolic",
            output->name, subtitle, online ? "ONLINE" : "OFFLINE",
            NULL, FALSE);

        const char *persistence_arguments[] = {
            "virtual-output", "set-persistent", output->name,
            output->persistent ? "temporary" : "persistent", NULL,
        };
        g_autofree char *persistence =
            anto_display_virtual_command(persistence_arguments);
        g_autofree char *persistence_key = g_strdup_printf(
            "virtual:output:%s:persistence", output->name);
        anto_display_spec_add(
            specs, DISPLAY_SECTION_VIRTUAL, persistence_key,
            output->persistent ? "document-revert-symbolic"
                               : "document-save-symbolic",
            output->persistent
                ? "Usa solo per questa sessione"
                : "Ripristina automaticamente all’avvio",
            output->persistent
                ? "Disattiva la persistenza senza rimuovere l’output attuale"
                : "Salva questo output nello stato locale della macchina",
            output->persistent ? "PERSISTE" : "SESSIONE",
            persistence, FALSE);

        const char *remove_arguments[] = {
            "virtual-output", "remove", output->name, NULL,
        };
        g_autofree char *remove = anto_display_virtual_command(remove_arguments);
        g_autofree char *remove_key = g_strdup_printf(
            "virtual:output:%s:remove", output->name);
        g_autofree char *remove_title =
            g_strdup_printf("Rimuovi %s", output->name);
        anto_display_spec_add(
            specs, DISPLAY_SECTION_VIRTUAL, remove_key,
            "user-trash-symbolic", remove_title,
            online
                ? "Rimuove l’output da Hyprland e dallo stato locale"
                : "Elimina la configurazione offline dallo stato locale",
            "RIMUOVI", remove, FALSE);
    }

    if (has_offline_persistent) {
        const char *restore_arguments[] = {
            "virtual-output", "restore", NULL,
        };
        g_autofree char *restore = anto_display_virtual_command(restore_arguments);
        anto_display_spec_add(
            specs, DISPLAY_SECTION_VIRTUAL, "virtual:restore",
            "view-refresh-symbolic", "Riporta online gli output persistenti",
            "Ricrea soltanto i monitor gestiti che risultano offline",
            "RIPRISTINA", restore, FALSE);
    }

    for (guint index = 0; index < G_N_ELEMENTS(presets); index++) {
        const char *create_arguments[] = {
            "virtual-output", "create", "auto",
            presets[index].width, presets[index].height,
            "60", "1", "persistent", NULL,
        };
        g_autofree char *create = anto_display_virtual_command(create_arguments);
        g_autofree char *preset_key =
            g_strdup_printf("virtual:preset:%s", presets[index].key);
        anto_display_spec_add(
            specs, DISPLAY_SECTION_VIRTUAL, preset_key,
            presets[index].icon, presets[index].title,
            "60 Hz · scala 100% · persistente", "CREA",
            create, FALSE);
    }
}

void anto_display_collect_monitor_specs(GPtrArray *specs,
                                  const DisplayMonitor *monitor,
                                  guint active_count) {
    g_autofree char *subtitle = anto_display_monitor_subtitle(monitor);
    g_autofree char *command = anto_display_command(
        monitor->enabled ? "focus" : "enable", monitor->name, NULL);
    g_autofree char *key =
        g_strdup_printf("connected:%s", monitor->name);
    const char *badge = !monitor->enabled ? "SPENTO" :
                        monitor->focused ? "FOCUS" :
                        (monitor->mirror && g_strcmp0(monitor->mirror, "none") != 0)
                            ? "MIRROR" : "ATTIVO";
    anto_display_spec_add(
        specs, DISPLAY_SECTION_CONNECTED, key, anto_display_output_icon(monitor),
        monitor->description, subtitle, badge,
        monitor->enabled && monitor->focused ? NULL : command, TRUE);

    g_autofree char *disable = anto_display_command("disable", monitor->name, NULL);
    g_autofree char *title = g_strdup_printf("Disattiva %s", monitor->name);
    g_autofree char *disable_key =
        g_strdup_printf("connected:%s:disable", monitor->name);
    DisplayRowSpec *disable_spec = anto_display_spec_add(
        specs, DISPLAY_SECTION_CONNECTED, disable_key,
        "video-display-symbolic", title,
        "Le finestre verranno spostate su un monitor rimasto attivo",
        "15 SEC", disable, TRUE);
    disable_spec->hidden = !monitor->enabled || active_count <= 1;
}

void anto_display_collect_scale_specs(GPtrArray *specs,
                                const DisplayMonitor *monitor) {
    static const struct {
        const char *value;
        const char *label;
    } scales[] = {
        {"auto", "Automatica"}, {"1", "100%"}, {"1.25", "125%"},
        {"1.5", "150%"}, {"1.6", "160%"}, {"2", "200%"},
    };
    int width = 0, height = 0;
    (void)sscanf(monitor->resolution, "%dx%d", &width, &height);
    for (guint i = 0; i < G_N_ELEMENTS(scales); i++) {
        double scale = g_ascii_strtod(scales[i].value, NULL);
        gboolean valid = TRUE;
        if (g_strcmp0(scales[i].value, "auto") != 0 && width > 0 && height > 0) {
            double logical_width = width / scale;
            double logical_height = height / scale;
            if (fabs(logical_width - round(logical_width)) > 0.0001 ||
                fabs(logical_height - round(logical_height)) > 0.0001)
                valid = FALSE;
        }
        g_autofree char *title = g_strdup_printf("%s · %s", monitor->name,
                                                 scales[i].label);
        gboolean current = monitor->enabled && valid &&
                           g_strcmp0(scales[i].value, "auto") != 0 &&
                           fabs(g_ascii_strtod(scales[i].value, NULL) -
                                g_ascii_strtod(monitor->scale, NULL)) < 0.001;
        g_autofree char *subtitle = g_strdup_printf(
            "Scala dell’interfaccia su %s%s", monitor->description,
            current ? " · valore attuale" : "");
        g_autofree char *command = anto_display_command("scale", monitor->name,
                                                   scales[i].value);
        g_autofree char *key = g_strdup_printf(
            "scale:%s:%s", monitor->name, scales[i].value);
        const char *badge = current ? "ATTUALE" : NULL;
        DisplayRowSpec *spec = anto_display_spec_add(
            specs, DISPLAY_SECTION_SCALE, key, "zoom-fit-best-symbolic",
            title, subtitle, badge, current ? NULL : command, TRUE);
        spec->hidden = !monitor->enabled || !valid;
    }
}

void anto_display_collect_transform_specs(GPtrArray *specs,
                                    const DisplayMonitor *monitor) {
    static const struct {
        int value;
        const char *label;
        const char *icon;
    } transforms[] = {
        {0, "Orizzontale", "object-rotate-left-symbolic"},
        {1, "Verticale · 90°", "object-rotate-right-symbolic"},
        {2, "Capovolto · 180°", "object-flip-vertical-symbolic"},
        {3, "Verticale · 270°", "object-rotate-left-symbolic"},
    };
    for (guint i = 0; i < G_N_ELEMENTS(transforms); i++) {
        g_autofree char *value = g_strdup_printf("%d", transforms[i].value);
        g_autofree char *title = g_strdup_printf("%s · %s", monitor->name,
                                                 transforms[i].label);
        g_autofree char *command = anto_display_command("transform", monitor->name,
                                                   value);
        g_autofree char *key = g_strdup_printf(
            "transform:%s:%d", monitor->name, transforms[i].value);
        const char *badge = monitor->transform == transforms[i].value ? "ATTUALE" : NULL;
        DisplayRowSpec *spec = anto_display_spec_add(
            specs, DISPLAY_SECTION_TRANSFORM, key, transforms[i].icon,
            title, monitor->transform == transforms[i].value
                       ? "Orientamento attualmente in uso"
                       : "Rotazione con ripristino automatico se non confermata",
            badge,
            monitor->transform == transforms[i].value ? NULL : command,
            TRUE);
        spec->hidden = !monitor->enabled;
    }
}

void anto_display_collect_mode_specs(GPtrArray *specs,
                               const DisplayMonitor *monitor) {
    if (!monitor->modes) return;
    g_autoptr(GPtrArray) chosen = anto_display_recommended_modes(monitor->modes, monitor);
    for (guint i = 0; i < chosen->len; i++) {
        DisplayMode *mode = g_ptr_array_index(chosen, i);
        g_autofree char *title = g_strdup_printf("%s · %d×%d a %.2f Hz",
                                                 monitor->name, mode->width,
                                                 mode->height, mode->rate);
        g_autofree char *subtitle = g_strdup_printf(
            "%s · risoluzione e frequenza validate dall’EDID",
            monitor->description);
        g_autofree char *command = anto_display_command("mode", monitor->name,
                                                   mode->text);
        int width = 0, height = 0;
        (void)sscanf(monitor->resolution, "%dx%d", &width, &height);
        double refresh = g_ascii_strtod(monitor->refresh, NULL);
        gboolean current = width == mode->width && height == mode->height &&
                           fabs(refresh - mode->rate) < 0.05;
        g_autofree char *key = g_strdup_printf(
            "mode:%s:%s", monitor->name, mode->text);
        anto_display_spec_add(
            specs, DISPLAY_SECTION_MODE, key, "video-display-symbolic",
            title, subtitle, current ? "ATTUALE" : NULL,
            current ? NULL : command, TRUE);
    }
}

void anto_display_collect_profile_specs(GPtrArray *specs,
                                  const DisplaySnapshot *snapshot) {
    g_autofree char *save = anto_display_command("persist-current", NULL, NULL);
    anto_display_spec_add(
        specs, DISPLAY_SECTION_PROFILE, "profile:persist-current",
        "document-save-symbolic", "Salva configurazione corrente",
        "Scrive atomicamente monitor, modalità, posizione, scala, rotazione e mirror per il prossimo avvio",
        NULL, save, FALSE);

    if (snapshot->has_last) {
        g_autofree char *last = anto_display_command("profile-apply", "last", NULL);
        anto_display_spec_add(
            specs, DISPLAY_SECTION_PROFILE, "profile:last",
            "document-revert-symbolic", "Ripristina ultimo layout salvato",
            "Riapplica monitor, posizione, scala e modalità dell’ultimo salvataggio riuscito",
            NULL, last, TRUE);
    }

    for (guint i = 0; snapshot->profiles &&
                        i < snapshot->profiles->len; i++) {
        DisplayProfile *profile = g_ptr_array_index(snapshot->profiles, i);
        g_autofree char *title =
            g_strdup_printf("Applica profilo · %s", profile->name);
        g_autofree char *command =
            anto_display_command("profile-apply", profile->name, NULL);
        g_autofree char *key =
            g_strdup_printf("profile:saved:%s", profile->name);
        anto_display_spec_add(
            specs, DISPLAY_SECTION_PROFILE, key, "view-restore-symbolic",
            title, profile->summary, "SALVATO", command, TRUE);
    }
}

GPtrArray *anto_display_collect_display_specs(const DisplaySnapshot *snapshot) {
    GPtrArray *specs = g_ptr_array_new_with_free_func(anto_display_row_spec_free);
    GPtrArray *monitors = snapshot->monitors;
    guint active_count = 0;
    guint regular_count = 0;
    for (guint i = 0; i < monitors->len; i++) {
        DisplayMonitor *monitor = g_ptr_array_index(monitors, i);
        if (monitor->enabled) active_count++;
        if (!anto_display_monitor_is_managed_virtual(snapshot, monitor))
            regular_count++;
    }

    if (regular_count == 0 &&
        (!snapshot->virtual_outputs ||
         snapshot->virtual_outputs->len == 0)) {
        anto_display_spec_add(
            specs, DISPLAY_SECTION_CONNECTED, "connected:none",
            "dialog-warning-symbolic", "Nessun monitor rilevato",
            "Controlla che la sessione Hyprland sia attiva",
            NULL, NULL, FALSE);
    } else {
        for (guint i = 0; i < monitors->len; i++) {
            DisplayMonitor *monitor = g_ptr_array_index(monitors, i);
            if (!anto_display_monitor_is_managed_virtual(snapshot, monitor))
                anto_display_collect_monitor_specs(specs, monitor, active_count);
        }
    }

    anto_display_collect_virtual_specs(specs, snapshot);

    if (monitors->len > 1) {
        g_autofree char *mirror =
            anto_display_command("mirror", NULL, NULL);
        DisplayRowSpec *mirror_spec = anto_display_spec_add(
            specs, DISPLAY_SECTION_LAYOUT, "layout:mirror",
            "edit-copy-symbolic", "Duplica gli schermi",
            "Usa come sorgente il pannello interno attivo o il monitor in uso",
            "15 SEC", mirror, TRUE);
        mirror_spec->hidden = anto_display_outputs_already_mirrored(monitors);
        for (guint i = 0; i < monitors->len; i++) {
            DisplayMonitor *monitor = g_ptr_array_index(monitors, i);
            g_autofree char *title =
                g_strdup_printf("Usa solo %s", monitor->name);
            g_autofree char *subtitle = g_strdup_printf(
                "Attiva %s e disattiva temporaneamente le altre uscite",
                monitor->description);
            g_autofree char *only =
                anto_display_command("only", monitor->name, NULL);
            g_autofree char *key =
                g_strdup_printf("layout:only:%s", monitor->name);
            DisplayRowSpec *spec = anto_display_spec_add(
                specs, DISPLAY_SECTION_LAYOUT, key, anto_display_output_icon(monitor),
                title, subtitle, "15 SEC", only, TRUE);
            spec->hidden = active_count == 1 && monitor->enabled;
        }
    }

    for (guint i = 0; i < monitors->len; i++) {
        DisplayMonitor *monitor = g_ptr_array_index(monitors, i);
        anto_display_collect_scale_specs(specs, monitor);
        anto_display_collect_transform_specs(specs, monitor);
        if (monitor->enabled)
            anto_display_collect_mode_specs(specs, monitor);
    }

    anto_display_collect_profile_specs(specs, snapshot);

    for (guint i = 0; i < monitors->len; i++) {
        DisplayMonitor *monitor = g_ptr_array_index(monitors, i);
        const char *action = monitor->dpms ? "dpms-off" : "dpms-on";
        g_autofree char *title = g_strdup_printf(
            "%s DPMS · %s", monitor->name,
            monitor->dpms ? "spegni" : "riaccendi");
        g_autofree char *subtitle = g_strdup_printf(
            "%s temporaneamente senza rimuoverlo dal layout",
            monitor->dpms ? "Oscura il monitor" : "Riattiva il monitor");
        g_autofree char *command =
            anto_display_command(action, monitor->name, NULL);
        g_autofree char *key =
            g_strdup_printf("tool:dpms:%s", monitor->name);
        DisplayRowSpec *spec = anto_display_spec_add(
            specs, DISPLAY_SECTION_TOOL, key,
            monitor->dpms ? "weather-clear-night-symbolic"
                          : "display-brightness-symbolic",
            title, subtitle, "TEMP", command, TRUE);
        spec->hidden = !monitor->enabled;
    }
    if (snapshot->editor) {
        g_autofree char *editor = anto_display_command("editor", NULL, NULL);
        anto_display_spec_add(
            specs, DISPLAY_SECTION_TOOL, "tool:editor",
            "preferences-desktop-display-symbolic", "Editor grafico",
            g_strcmp0(snapshot->editor, "nwg-displays") == 0
                ? "Apre nwg-displays" : "Apre wdisplays",
            NULL, editor, TRUE);
    }
    return specs;
}

void anto_display_scroll_restore_free(gpointer data) {
    DisplayScrollRestore *restore = data;
    if (!restore) return;
    g_weak_ref_clear(&restore->window);
    g_clear_object(&restore->adjustment);
    g_free(restore);
}

DisplayArranger *anto_display_unsafe_arranger(MenuApp *app) {
    DisplayArranger *arranger =
        anto_display_active_arranger && anto_display_active_arranger->app == app ? anto_display_active_arranger : NULL;
    if (!arranger || (!arranger->drag_active && !arranger->applying &&
                      !anto_display_arranger_has_changes(arranger)))
        return NULL;
    return arranger;
}

GtkListBoxRow *anto_display_first_visible_selectable(DisplayView *view) {
    for (int index = 0;; index++) {
        GtkListBoxRow *row = gtk_list_box_get_row_at_index(
            GTK_LIST_BOX(view->app->list), index);
        if (!row) return NULL;
        if (gtk_widget_get_visible(GTK_WIDGET(row)) &&
            gtk_list_box_row_get_selectable(row))
            return row;
    }
}

void anto_display_show_error_in_place(DisplayRuntime *runtime,
                                        const char *message) {
    DisplayView *view = runtime->view;
    if (!view) return;
    view->ready = FALSE;
    gtk_label_set_text(GTK_LABEL(view->app->page_subtitle),
                       "Snapshot non disponibile");
    DisplayRowSpec error = {
        .key = "fixed:loading",
        .section = DISPLAY_SECTION_CONNECTED,
        .icon = "dialog-warning-symbolic",
        .title = "Lettura non riuscita",
        .subtitle = (char *)(message && *message ? message :
            "Hyprland o l’adattatore display non sono raggiungibili"),
        .badge = "RETRY",
    };
    anto_display_row_update(view->loading, &error);
    anto_display_search(view->app, gtk_editable_get_text(
                       GTK_EDITABLE(view->app->search)), view);
    menu_set_footer(
        view->app, "Il listener riproverà al prossimo evento display");
}

void anto_display_apply_snapshot_in_place(DisplayRuntime *runtime) {
    DisplayView *view = runtime->view;
    if (!view || !runtime->snapshot) return;
    DisplaySnapshot *snapshot = runtime->snapshot;
    GPtrArray *monitors = snapshot->monitors;
    GtkAdjustment *adjustment = gtk_scrolled_window_get_vadjustment(
        GTK_SCROLLED_WINDOW(view->app->list_scroll));
    double scroll_value = gtk_adjustment_get_value(adjustment);

    guint active_count = 0;
    guint virtual_online = 0;
    GString *names = g_string_new(NULL);
    for (guint i = 0; i < monitors->len; i++) {
        DisplayMonitor *monitor = g_ptr_array_index(monitors, i);
        if (monitor->enabled) active_count++;
        if (names->len) g_string_append(names, " + ");
        g_string_append(names, monitor->name);
    }
    for (guint index = 0;
         snapshot->virtual_outputs &&
         index < snapshot->virtual_outputs->len;
         index++) {
        MenuVirtualOutput *output =
            g_ptr_array_index(snapshot->virtual_outputs, index);
        if (anto_display_monitor_named(monitors, output->name))
            virtual_online++;
    }
    g_autofree char *status = NULL;
    if (snapshot->virtual_outputs && snapshot->virtual_outputs->len > 0) {
        status = monitors->len
            ? g_strdup_printf(
                  "%u attivi su %u · virtuali %u/%u online · %s",
                  active_count, monitors->len, virtual_online,
                  snapshot->virtual_outputs->len, names->str)
            : g_strdup_printf(
                  "Virtuali %u/%u online", virtual_online,
                  snapshot->virtual_outputs->len);
    } else {
        status = monitors->len
            ? g_strdup_printf("%u attivi su %u · %s", active_count,
                              monitors->len, names->str)
            : g_strdup("Nessun monitor rilevato");
    }
    g_string_free(names, TRUE);
    if (g_strcmp0(gtk_label_get_text(GTK_LABEL(view->app->page_subtitle)),
                  status) != 0)
        gtk_label_set_text(GTK_LABEL(view->app->page_subtitle), status);

    anto_display_arranger_sync_monitors(view->arranger, monitors);
    view->arranger->refresh_pending = FALSE;
    g_autoptr(GPtrArray) specs = anto_display_collect_display_specs(snapshot);
    anto_display_reconcile_rows(view, specs);
    view->ready = TRUE;
    anto_display_search(view->app, gtk_editable_get_text(
                       GTK_EDITABLE(view->app->search)), view);
    menu_set_footer(
        view->app, snapshot->partial
                 ? "Stato monitor live · alcune modalità o profili non hanno risposto"
                 : "Le modifiche vengono salvate dopo la conferma · Esc chiude");
    anto_display_schedule_scroll_restore(view->app, scroll_value);
    runtime->snapshot_dirty = FALSE;
}

void anto_display_snapshot_finished(GObject *object, GAsyncResult *result,
                                      gpointer data) {
    (void)object;
    DisplayRuntime *runtime = data;
    g_autoptr(GError) error = NULL;
    DisplaySnapshot *snapshot =
        g_task_propagate_pointer(G_TASK(result), &error);
    runtime->in_flight = FALSE;

    GObject *window = g_weak_ref_get(&runtime->window);
    gboolean on_page =
        window && g_strcmp0(runtime->app->current_page, "display") == 0;
    if (snapshot) {
        anto_display_snapshot_free(runtime->snapshot);
        runtime->snapshot = snapshot;
        runtime->snapshot_dirty = TRUE;
    }

    if (on_page && runtime->snapshot_dirty)
        anto_display_render_if_safe(runtime);
    else if (on_page && !runtime->snapshot && error &&
             !anto_display_unsafe_arranger(runtime->app))
        anto_display_show_error_in_place(runtime, error->message);

    gboolean repeat = runtime->refresh_pending;
    runtime->refresh_pending = FALSE;
    if (repeat && on_page) {
        DisplayArranger *arranger = anto_display_unsafe_arranger(runtime->app);
        if (arranger) arranger->refresh_pending = TRUE;
        else anto_display_refresh_start(runtime);
    }

    g_clear_object(&window);
    anto_display_runtime_unref(runtime);
}

void anto_display_refresh_start(DisplayRuntime *runtime) {
    if (!runtime) return;
    if (runtime->in_flight) {
        runtime->refresh_pending = TRUE;
        return;
    }
    runtime->in_flight = TRUE;
    g_autofree char *path = anto_display_action_path();
    GTask *task = g_task_new(
        NULL, NULL, anto_display_snapshot_finished, anto_display_runtime_ref(runtime));
    g_task_set_task_data(task, g_strdup(path), g_free);
    g_task_run_in_thread(task, anto_display_snapshot_load);
    g_object_unref(task);
}

void menu_display_live_event(MenuApp *app) {
    if (!app || !app->window ||
        g_strcmp0(app->current_page, "display") != 0)
        return;
    DisplayRuntime *runtime = anto_display_runtime_get(app);
    if (!runtime) return;

    DisplayArranger *arranger = anto_display_unsafe_arranger(app);
    if (arranger) {
        arranger->refresh_pending = TRUE;
        if (runtime->in_flight) runtime->refresh_pending = TRUE;
        return;
    }

    if (runtime->snapshot_dirty)
        anto_display_render_if_safe(runtime);
    anto_display_refresh_start(runtime);
}
