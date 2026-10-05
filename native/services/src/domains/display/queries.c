#include "internal.h"
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

json_object *anto_display_monitor_json(GError **error) {
    const char *fixture_json = g_getenv("ANTO_DISPLAY_MONITORS_JSON");
    json_object *monitors = NULL;
    if (fixture_json && *fixture_json) {
        monitors = anto_display_parse_json_text(fixture_json, error);
    } else {
        const char *fixture_file = g_getenv("ANTO_DISPLAY_MONITORS_JSON_FILE");
        if (fixture_file && *fixture_file) {
            monitors = anto_display_load_json_file(fixture_file, error);
        } else {
            const char *hyprctl = backend_program("ANTO_MENU_HYPRCTL", "hyprctl");
            const char *argv[] = {hyprctl, "monitors", "all", "-j", NULL};
            BackendCommand command = backend_command_run(argv, NULL);
            if (command.status == 0)
                monitors = anto_display_parse_json_text(command.stdout_text, error);
            else
                g_set_error(error, G_IO_ERROR, G_IO_ERROR_FAILED,
                            "Hyprland non e' raggiungibile: %s",
                            command.stderr_text && *command.stderr_text
                                ? command.stderr_text : "hyprctl fallito");
            backend_command_clear(&command);
        }
    }
    if (!monitors) return NULL;
    if (!json_object_is_type(monitors, json_type_array)) {
        json_object_put(monitors);
        g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA,
                            "Risposta monitor non valida");
        return NULL;
    }
    return monitors;
}

int anto_display_action_status(json_object *monitors) {
    const size_t connected = json_object_array_length(monitors);
    GString *summary = g_string_new(NULL);
    for (size_t index = 0; index < connected; index++) {
        json_object *monitor = json_object_array_get_idx(monitors, index);
        if (anto_display_member_boolean(monitor, "disabled", FALSE)) continue;
        g_autofree char *rate = anto_display_format_number(
            round(anto_display_member_double(monitor, "refreshRate", 0)));
        if (summary->len) g_string_append(summary, " · ");
        g_string_append_printf(summary, "%s %" G_GINT64_FORMAT "x%" G_GINT64_FORMAT
                              "@%sHz", anto_display_member_string(monitor, "name", "?"),
                              anto_display_member_integer(monitor, "width", 0),
                              anto_display_member_integer(monitor, "height", 0), rate);
    }
    g_print("%u attivi su %zu collegati%s%s\n", anto_display_active_count(monitors), connected,
            summary->len ? " · " : "", summary->str);
    g_string_free(summary, TRUE);
    return 0;
}

int anto_display_action_list(json_object *monitors) {
    const size_t connected = json_object_array_length(monitors);
    for (size_t index = 0; index < connected; index++) {
        json_object *monitor = json_object_array_get_idx(monitors, index);
        const char *name = anto_display_member_string(monitor, "name", "");
        if (!anto_display_valid_output_name(name)) continue;
        gboolean disabled = anto_display_member_boolean(monitor, "disabled", FALSE);
        gint64 width = anto_display_member_integer(monitor, "width", 0);
        gint64 height = anto_display_member_integer(monitor, "height", 0);
        g_autofree char *description = backend_clean_field(
            anto_display_member_string(monitor, "description", name));
        g_autofree char *resolution = width > 0
            ? g_strdup_printf("%" G_GINT64_FORMAT "x%" G_GINT64_FORMAT,
                              width, height)
            : g_strdup("—");
        g_autofree char *rate = anto_display_format_number(
            anto_display_member_double(monitor, "refreshRate", 0));
        g_autofree char *scale = anto_display_format_number(
            anto_display_member_double(monitor, "scale", 1));
        g_autofree char *position = disabled
            ? g_strdup("—")
            : g_strdup_printf("%" G_GINT64_FORMAT ",%" G_GINT64_FORMAT,
                              anto_display_member_integer(monitor, "x", 0),
                              anto_display_member_integer(monitor, "y", 0));
        json_object *workspace = NULL;
        const char *workspace_name = "—";
        if (json_object_object_get_ex(monitor, "activeWorkspace", &workspace) &&
            workspace && json_object_is_type(workspace, json_type_object))
            workspace_name = anto_display_member_string(workspace, "name", "—");
        json_object *modes = NULL;
        size_t mode_count = 0;
        if (json_object_object_get_ex(monitor, "availableModes", &modes) &&
            modes && json_object_is_type(modes, json_type_array))
            mode_count = json_object_array_length(modes);
        g_print("%s\t%s\t%s\t%s\t%s\t%s\t%" G_GINT64_FORMAT
                "\t%s\t%s\t%s\t%s\t%s\t%zu\n",
                name, description, disabled ? "off" : "on", resolution,
                rate, scale, anto_display_member_integer(monitor, "transform", 0),
                position, workspace_name,
                anto_display_member_boolean(monitor, "focused", FALSE) ? "true" : "false",
                anto_display_mirror_for(monitor),
                anto_display_member_boolean(monitor, "dpmsStatus", FALSE) ? "true" : "false",
                mode_count);
    }
    return 0;
}

int anto_display_action_modes(json_object *monitors, const char *name) {
    g_autoptr(GError) error = NULL;
    if (!anto_display_require_output(monitors, name, &error))
        return anto_display_display_error("output", anto_display_display_error_message(error));
    json_object *monitor = anto_display_find_output(monitors, name);
    json_object *modes = NULL;
    if (!json_object_object_get_ex(monitor, "availableModes", &modes) ||
        !modes || !json_object_is_type(modes, json_type_array))
        return 0;
    GHashTable *seen = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
    const size_t count = json_object_array_length(modes);
    for (size_t index = 0; index < count; index++) {
        json_object *value = json_object_array_get_idx(modes, index);
        if (!value || !json_object_is_type(value, json_type_string)) continue;
        g_autofree char *mode = g_strdup(json_object_get_string(value));
        if (g_str_has_suffix(mode, "Hz")) mode[strlen(mode) - 2] = '\0';
        if (!g_hash_table_contains(seen, mode)) {
            g_print("%s\n", mode);
            g_hash_table_add(seen, g_strdup(mode));
        }
    }
    g_hash_table_unref(seen);
    return 0;
}

int anto_display_action_focus(DisplayContext *context, json_object *monitors,
                        const char *name) {
    g_autoptr(GError) error = NULL;
    if (!anto_display_require_output(monitors, name, &error))
        return anto_display_display_error("output", anto_display_display_error_message(error));
    if (!anto_display_run_hypr(context, "dispatch", "focusmonitor", name, &error))
        return anto_display_display_error("hyprctl", anto_display_display_error_message(error));
    return 0;
}

int anto_display_action_dpms(DisplayContext *context, json_object *monitors,
                       const char *verb, const char *name) {
    g_autoptr(GError) error = NULL;
    if (name && *name && !anto_display_require_output(monitors, name, &error))
        return anto_display_display_error("output", anto_display_display_error_message(error));
    g_autofree char *argument = name && *name
                                    ? g_strdup_printf("%s %s", verb, name)
                                    : g_strdup(verb);
    if (!anto_display_run_hypr(context, "dispatch", "dpms", argument, &error))
        return anto_display_display_error("hyprctl", anto_display_display_error_message(error));
    return 0;
}

int anto_display_action_editor(void) {
    const char *editors[] = {"nwg-displays", "wdisplays", NULL};
    for (guint index = 0; editors[index]; index++) {
        g_autofree char *path = g_find_program_in_path(editors[index]);
        if (!path) continue;
        const char *argv[] = {path, NULL};
        g_autoptr(GError) error = NULL;
        if (g_spawn_async(NULL, (char **)argv, NULL, G_SPAWN_SEARCH_PATH,
                          NULL, NULL, NULL, &error))
            return 0;
        return anto_display_display_error("editor", anto_display_display_error_message(error));
    }
    return anto_display_display_error("editor",
                         "Installa nwg-displays o wdisplays per l'editor grafico");
}

int anto_display_action_help(void) {
    g_print(
        "Uso: anto-menu-backend display status|list|modes OUTPUT\n"
        "     display enable|disable|toggle|only OUTPUT\n"
        "     display extend DIR | mirror [OUTPUT]\n"
        "     display scale|transform|mode|position OUTPUT VALUE\n"
        "     display arrange JSON | focus OUTPUT | dpms-on|dpms-off [OUTPUT]\n"
        "     display persistent-preview|persist-current\n"
        "     display profile-save|profile-apply|profile-delete [NAME]\n");
    return 0;
}
