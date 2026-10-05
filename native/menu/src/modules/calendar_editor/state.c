#include "internal.h"

char *anto_calendar_editor_calendar_adapter(void) {
    const char *override = g_getenv("ANTO_MENU_ACTIONS_DIR");
    if (override && *override)
        return g_build_filename(override, "calendar.sh", NULL);
    g_autofree char *installed = menu_home_path(
        ".local/lib/anto-menu/actions/calendar.sh");
    if (g_file_test(installed, G_FILE_TEST_IS_EXECUTABLE))
        return g_steal_pointer(&installed);
    return menu_config_path("anto426/native-menu/scripts/actions/calendar.sh");
}

void anto_calendar_editor_editor_status(CalendarEditor *editor, const char *message,
                          gboolean error) {
    gtk_label_set_text(GTK_LABEL(editor->status), message ? message : "");
    gtk_widget_remove_css_class(editor->status, "success");
    gtk_widget_remove_css_class(editor->status, "error");
    if (message && *message)
        gtk_widget_add_css_class(editor->status, error ? "error" : "success");
}

gboolean anto_calendar_editor_valid_time(const char *value) {
    if (!value || strlen(value) != 5 || value[2] != ':') return FALSE;
    if (!g_ascii_isdigit(value[0]) || !g_ascii_isdigit(value[1]) ||
        !g_ascii_isdigit(value[3]) || !g_ascii_isdigit(value[4]))
        return FALSE;
    int hour = (value[0] - '0') * 10 + value[1] - '0';
    int minute = (value[3] - '0') * 10 + value[4] - '0';
    return hour >= 0 && hour <= 23 && minute >= 0 && minute <= 59;
}

void anto_calendar_editor_return_to_calendar(CalendarEditor *editor) {
    if (editor->app->history && editor->app->history->len) {
        const char *previous = g_ptr_array_index(
            editor->app->history, editor->app->history->len - 1);
        if (g_strcmp0(previous, "calendar") == 0) {
            menu_back(editor->app);
            return;
        }
    }
    menu_open(editor->app, "calendar");
}

void anto_calendar_editor_save_event(GtkButton *button, gpointer data) {
    CalendarEditor *editor = data;
    const char *title = gtk_editable_get_text(GTK_EDITABLE(editor->title));
    g_autofree char *trimmed_title = g_strdup(title ? title : "");
    g_strstrip(trimmed_title);
    gboolean all_day = gtk_switch_get_active(GTK_SWITCH(editor->all_day));
    const char *start = gtk_editable_get_text(GTK_EDITABLE(editor->start));
    const char *end = gtk_editable_get_text(GTK_EDITABLE(editor->end));
    gboolean sync = GPOINTER_TO_INT(
        g_object_get_data(G_OBJECT(button), "calendar-sync"));

    if (!*trimmed_title) {
        anto_calendar_editor_editor_status(editor, "Scrivi un titolo per l’evento", TRUE);
        gtk_widget_grab_focus(editor->title);
        return;
    }
    if (!all_day && !anto_calendar_editor_valid_time(start)) {
        anto_calendar_editor_editor_status(editor, "Ora di inizio non valida · usa HH:MM", TRUE);
        gtk_widget_grab_focus(editor->start);
        return;
    }
    if (!all_day && *end && (!anto_calendar_editor_valid_time(end) || g_strcmp0(end, start) <= 0)) {
        anto_calendar_editor_editor_status(editor, "L’ora di fine deve seguire l’inizio", TRUE);
        gtk_widget_grab_focus(editor->end);
        return;
    }

    g_autoptr(GDateTime) selected = gtk_calendar_get_date(editor->calendar);
    g_autofree char *date = selected
                                ? g_date_time_format(selected, "%Y-%m-%d")
                                : g_strdup("");
    GtkTextIter first;
    GtkTextIter last;
    gtk_text_buffer_get_bounds(editor->description, &first, &last);
    g_autofree char *description = gtk_text_buffer_get_text(
        editor->description, &first, &last, FALSE);

    struct json_object *event = json_object_new_object();
    json_object_object_add(event, "title", json_object_new_string(trimmed_title));
    json_object_object_add(event, "date", json_object_new_string(date));
    json_object_object_add(event, "start",
                           json_object_new_string(all_day ? "" : start));
    json_object_object_add(event, "end",
                           json_object_new_string(all_day ? "" : end));
    json_object_object_add(event, "description",
                           json_object_new_string(description));
    json_object_object_add(event, "all_day", json_object_new_boolean(all_day));
    const char *payload = json_object_to_json_string_ext(
        event, JSON_C_TO_STRING_PLAIN);

    g_autofree char *adapter = anto_calendar_editor_calendar_adapter();
    const char *argv[] = {adapter, "add", NULL};
    char *stdout_text = NULL;
    char *stderr_text = NULL;
    gboolean saved = menu_run_with_input(argv, payload, &stdout_text, &stderr_text);
    json_object_put(event);
    g_autofree char *out = stdout_text;
    g_autofree char *error = stderr_text;
    if (!saved) {
        if (error) g_strstrip(error);
        anto_calendar_editor_editor_status(editor, error && *error ? error :
                          "Impossibile salvare l’evento locale", TRUE);
        return;
    }

    anto_calendar_editor_editor_status(editor, "Evento salvato nell’agenda locale", FALSE);
    if (sync) {
        const char *sync_argv[] = {adapter, "sync", NULL};
        menu_spawn(editor->app, sync_argv, FALSE);
    }
    anto_calendar_editor_return_to_calendar(editor);
}
