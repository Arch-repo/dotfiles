#include "internal.h"

void anto_display_arranger_reset_clicked(GtkButton *button, gpointer data) {
    (void)button;
    DisplayArranger *arranger = data;
    for (guint i = 0; i < arranger->monitors->len; i++) {
        ArrangeMonitor *monitor = g_ptr_array_index(arranger->monitors, i);
        monitor->x = monitor->initial_x;
        monitor->y = monitor->initial_y;
    }
    anto_display_arranger_update_controls(arranger);
    gtk_widget_queue_draw(arranger->area);
    anto_display_arranger_flush_deferred_refresh(arranger);
}

void anto_display_arranger_apply_clicked(GtkButton *button, gpointer data) {
    (void)button;
    DisplayArranger *arranger = data;
    if (!anto_display_arranger_has_changes(arranger)) return;
    struct json_object *plan = json_object_new_array();
    for (guint i = 0; i < arranger->monitors->len; i++) {
        ArrangeMonitor *monitor = g_ptr_array_index(arranger->monitors, i);
        struct json_object *entry = json_object_new_object();
        json_object_object_add(entry, "name", json_object_new_string(monitor->name));
        json_object_object_add(entry, "x",
                               json_object_new_int64((int64_t)llround(monitor->x)));
        json_object_object_add(entry, "y",
                               json_object_new_int64((int64_t)llround(monitor->y)));
        json_object_array_add(plan, entry);
    }
    const char *json = json_object_to_json_string_ext(plan, JSON_C_TO_STRING_PLAIN);
    g_autofree char *command = anto_display_command("arrange", json, NULL);
    gtk_label_set_text(GTK_LABEL(arranger->selection_label),
                       "Applicazione e salvataggio del layout in corso…");
    arranger->applying = TRUE;
    gtk_widget_set_sensitive(arranger->apply_button, FALSE);
    menu_spawn_shell(arranger->app, command, TRUE);
    json_object_put(plan);
}
