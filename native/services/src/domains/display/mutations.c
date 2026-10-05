#include "internal.h"
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

gboolean anto_display_run_hypr(DisplayContext *context, const char *first,
                         const char *second, const char *third,
                         GError **error) {
    if (context->dry_run || context->mock_apply) {
        g_print("%s hyprctl %s%s%s%s%s\n",
                context->dry_run ? "DRY-RUN" : "MOCK", first,
                second ? " " : "", second ? second : "",
                third ? " " : "", third ? third : "");
        context->mutation_applied = TRUE;
        return TRUE;
    }
    const char *hyprctl = backend_program("ANTO_MENU_HYPRCTL", "hyprctl");
    const char *argv[] = {hyprctl, first, second, third, NULL};
    BackendCommand command = backend_command_run(argv, NULL);
    gboolean ok = command.status == 0;
    if (!ok)
        g_set_error(error, G_IO_ERROR, G_IO_ERROR_FAILED, "%s",
                    command.stderr_text && *command.stderr_text
                        ? command.stderr_text : "hyprctl non riuscito");
    else
        context->mutation_applied = TRUE;
    backend_command_clear(&command);
    return ok;
}

static gboolean apply_rule(DisplayContext *context, const char *rule,
                           GError **error) {
    return anto_display_run_hypr(context, "keyword", "monitor", rule, error);
}

gboolean anto_display_apply_output(DisplayContext *context, const char *name,
                             const char *mode, const char *position,
                             const char *scale, gint64 transform,
                             const char *mirror, GError **error) {
    g_autofree char *rule = NULL;
    if (mirror && *mirror && g_strcmp0(mirror, "none") != 0)
        rule = g_strdup_printf("%s,%s,%s,%s,transform,%" G_GINT64_FORMAT
                               ",mirror,%s", name, mode, position, scale,
                               transform, mirror);
    else
        rule = g_strdup_printf("%s,%s,%s,%s,transform,%" G_GINT64_FORMAT,
                               name, mode, position, scale, transform);
    return apply_rule(context, rule, error);
}

static gboolean disable_output(DisplayContext *context, const char *name,
                               GError **error) {
    g_autofree char *rule = g_strdup_printf("%s,disable", name);
    return apply_rule(context, rule, error);
}

gboolean anto_display_restore_snapshot(DisplayContext *context, json_object *profile,
                                 GError **error) {
    if (!profile || !json_object_is_type(profile, json_type_array) ||
        json_object_array_length(profile) == 0) {
        g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA,
                            "Profilo monitor danneggiato");
        return FALSE;
    }
    json_object *current = anto_display_monitor_json(error);
    if (!current) return FALSE;
    guint enabled_connected = 0;
    const size_t count = json_object_array_length(profile);
    for (size_t index = 0; index < count; index++) {
        json_object *entry = json_object_array_get_idx(profile, index);
        const char *name = anto_display_member_string(entry, "name", "");
        if (!anto_display_member_boolean(entry, "disabled", FALSE) &&
            anto_display_find_output(current, name))
            enabled_connected++;
    }
    if (enabled_connected == 0) {
        json_object_put(current);
        g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_FAILED,
                            "Il profilo non lascerebbe monitor attivi");
        return FALSE;
    }
    gboolean ok = TRUE;
    for (guint phase = 0; phase < 3 && ok; phase++) {
        for (size_t index = 0; index < count && ok; index++) {
            json_object *entry = json_object_array_get_idx(profile, index);
            const char *name = anto_display_member_string(entry, "name", "");
            if (!anto_display_find_output(current, name)) continue;
            gboolean disabled = anto_display_member_boolean(entry, "disabled", FALSE);
            gboolean mirrored = g_strcmp0(anto_display_mirror_for(entry), "none") != 0;
            guint entry_phase = disabled ? 2U : mirrored ? 1U : 0U;
            if (entry_phase != phase) continue;
            if (disabled) {
                ok = disable_output(context, name, error);
            } else {
                g_autofree char *mode = anto_display_mode_for(entry);
                g_autofree char *position = anto_display_position_for(entry);
                g_autofree char *scale = anto_display_format_number(
                    anto_display_member_double(entry, "scale", 1));
                ok = anto_display_apply_output(context, name, mode, position, scale,
                                  anto_display_member_integer(entry, "transform", 0),
                                  anto_display_mirror_for(entry), error);
            }
        }
    }
    json_object_put(current);
    return ok;
}

gboolean anto_display_mutation_enable(DisplayContext *context, json_object *monitors,
                                const char *name, const char *unused,
                                GError **error) {
    (void)unused;
    if (!anto_display_require_output(monitors, name, error)) return FALSE;
    json_object *monitor = anto_display_find_output(monitors, name);
    g_autofree char *mode = anto_display_mode_for(monitor);
    g_autofree char *scale = g_strcmp0(mode, "preferred") == 0
                                 ? g_strdup("auto")
                                 : anto_display_format_number(anto_display_member_double(monitor, "scale", 1));
    return anto_display_apply_output(context, name, mode, "auto-right", scale,
                        anto_display_member_integer(monitor, "transform", 0), "none", error);
}

gboolean anto_display_mutation_disable(DisplayContext *context, json_object *monitors,
                                 const char *name, const char *unused,
                                 GError **error) {
    (void)unused;
    if (!anto_display_require_output(monitors, name, error)) return FALSE;
    json_object *monitor = anto_display_find_output(monitors, name);
    if (anto_display_member_boolean(monitor, "disabled", FALSE)) {
        g_set_error(error, G_IO_ERROR, G_IO_ERROR_FAILED,
                    "%s e' gia' disattivato", name);
        return FALSE;
    }
    if (anto_display_active_count(monitors) <= 1) {
        g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_FAILED,
                            "Protezione attiva: non puoi disattivare l'unico monitor");
        return FALSE;
    }
    return disable_output(context, name, error);
}

gboolean anto_display_mutation_toggle(DisplayContext *context, json_object *monitors,
                                const char *name, const char *unused,
                                GError **error) {
    if (!anto_display_require_output(monitors, name, error)) return FALSE;
    return anto_display_member_boolean(anto_display_find_output(monitors, name), "disabled", FALSE)
               ? anto_display_mutation_enable(context, monitors, name, unused, error)
               : anto_display_mutation_disable(context, monitors, name, unused, error);
}

gboolean anto_display_mutation_only(DisplayContext *context, json_object *monitors,
                              const char *name, const char *unused,
                              GError **error) {
    (void)unused;
    if (!anto_display_require_output(monitors, name, error)) return FALSE;
    json_object *wanted = anto_display_find_output(monitors, name);
    g_autofree char *mode = anto_display_mode_for(wanted);
    g_autofree char *scale = g_strcmp0(mode, "preferred") == 0
                                 ? g_strdup("auto")
                                 : anto_display_format_number(anto_display_member_double(wanted, "scale", 1));
    if (!anto_display_apply_output(context, name, mode, "0x0", scale,
                      anto_display_member_integer(wanted, "transform", 0), "none", error))
        return FALSE;
    const size_t count = json_object_array_length(monitors);
    for (size_t index = 0; index < count; index++) {
        const char *other = anto_display_member_string(
            json_object_array_get_idx(monitors, index), "name", "");
        if (g_strcmp0(other, name) != 0 &&
            !disable_output(context, other, error))
            return FALSE;
    }
    return TRUE;
}

gboolean anto_display_mutation_extend(DisplayContext *context, json_object *monitors,
                                const char *direction, const char *unused,
                                GError **error) {
    (void)unused;
    if (!anto_display_valid_direction(direction)) {
        g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
                            "Direzione layout non valida");
        return FALSE;
    }
    if (json_object_array_length(monitors) < 2) {
        g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_FAILED,
                            "Serve almeno un secondo monitor collegato");
        return FALSE;
    }
    const char *primary = anto_display_primary_output(monitors);
    const size_t count = json_object_array_length(monitors);
    for (guint phase = 0; phase < 2; phase++) {
        for (size_t index = 0; index < count; index++) {
            json_object *monitor = json_object_array_get_idx(monitors, index);
            const char *name = anto_display_member_string(monitor, "name", "");
            if ((phase == 0) != (g_strcmp0(name, primary) == 0)) continue;
            g_autofree char *mode = anto_display_mode_for(monitor);
            g_autofree char *scale = g_strcmp0(mode, "preferred") == 0
                                         ? g_strdup("auto")
                                         : anto_display_format_number(anto_display_member_double(monitor, "scale", 1));
            g_autofree char *position = phase == 0
                ? g_strdup("0x0") : g_strdup_printf("auto-%s", direction);
            if (!anto_display_apply_output(context, name, mode, position, scale,
                              anto_display_member_integer(monitor, "transform", 0), "none",
                              error))
                return FALSE;
        }
    }
    return TRUE;
}

gboolean anto_display_mutation_mirror(DisplayContext *context, json_object *monitors,
                                const char *requested, const char *unused,
                                GError **error) {
    (void)unused;
    if (json_object_array_length(monitors) < 2) {
        g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_FAILED,
                            "Serve almeno un secondo monitor collegato");
        return FALSE;
    }
    const char *source = requested && *requested ? requested : anto_display_primary_output(monitors);
    if (!anto_display_require_output(monitors, source, error)) return FALSE;
    json_object *source_monitor = anto_display_find_output(monitors, source);
    g_autofree char *source_mode = anto_display_mode_for(source_monitor);
    g_autofree char *source_scale = g_strcmp0(source_mode, "preferred") == 0
        ? g_strdup("auto")
        : anto_display_format_number(anto_display_member_double(source_monitor, "scale", 1));
    if (!anto_display_apply_output(context, source, source_mode, "0x0", source_scale,
                      anto_display_member_integer(source_monitor, "transform", 0), "none",
                      error))
        return FALSE;
    const size_t count = json_object_array_length(monitors);
    for (size_t index = 0; index < count; index++) {
        json_object *monitor = json_object_array_get_idx(monitors, index);
        const char *name = anto_display_member_string(monitor, "name", "");
        if (g_strcmp0(name, source) == 0) continue;
        g_autofree char *mode = anto_display_mode_for(monitor);
        g_autofree char *scale = g_strcmp0(mode, "preferred") == 0
                                     ? g_strdup("auto")
                                     : anto_display_format_number(anto_display_member_double(monitor, "scale", 1));
        if (!anto_display_apply_output(context, name, mode, "auto", scale,
                          anto_display_member_integer(monitor, "transform", 0), source,
                          error))
            return FALSE;
    }
    return TRUE;
}

gboolean anto_display_mutation_scale(DisplayContext *context, json_object *monitors,
                               const char *name, const char *scale,
                               GError **error) {
    double unused = 0;
    if (!anto_display_parse_scale(scale, &unused)) {
        g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
                            "La scala deve essere compresa tra 0.5 e 4");
        return FALSE;
    }
    if (!anto_display_require_output(monitors, name, error)) return FALSE;
    json_object *monitor = anto_display_find_output(monitors, name);
    g_autofree char *mode = anto_display_mode_for(monitor);
    if (!anto_display_scale_fits_mode(mode, scale)) {
        g_set_error(error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
                    "Scala %s non valida per %s: pixel logici frazionari",
                    scale, mode);
        return FALSE;
    }
    g_autofree char *position = anto_display_position_for(monitor);
    return anto_display_apply_output(context, name, mode, position, scale,
                        anto_display_member_integer(monitor, "transform", 0),
                        anto_display_mirror_for(monitor), error);
}

gboolean anto_display_mutation_transform(DisplayContext *context,
                                   json_object *monitors, const char *name,
                                   const char *transform_text, GError **error) {
    if (!transform_text || strlen(transform_text) != 1 ||
        transform_text[0] < '0' || transform_text[0] > '7') {
        g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
                            "Rotazione non valida");
        return FALSE;
    }
    if (!anto_display_require_output(monitors, name, error)) return FALSE;
    json_object *monitor = anto_display_find_output(monitors, name);
    g_autofree char *mode = anto_display_mode_for(monitor);
    g_autofree char *position = anto_display_position_for(monitor);
    g_autofree char *scale = anto_display_format_number(anto_display_member_double(monitor, "scale", 1));
    return anto_display_apply_output(context, name, mode, position, scale,
                        transform_text[0] - '0', anto_display_mirror_for(monitor), error);
}

gboolean anto_display_mutation_mode(DisplayContext *context, json_object *monitors,
                              const char *name, const char *requested,
                              GError **error) {
    if (!anto_display_require_output(monitors, name, error)) return FALSE;
    gboolean special = g_strcmp0(requested, "preferred") == 0 ||
                       g_strcmp0(requested, "highres") == 0 ||
                       g_strcmp0(requested, "highrr") == 0;
    int width = 0;
    int height = 0;
    double rate = 0;
    json_object *monitor = anto_display_find_output(monitors, name);
    if (!special && (!anto_display_parse_mode(requested, &width, &height, &rate) ||
                     !anto_display_mode_available(monitor, requested))) {
        g_set_error(error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
                    "Modalita' %s non supportata da %s",
                    requested ? requested : "", name);
        return FALSE;
    }
    g_autofree char *position = anto_display_position_for(monitor);
    g_autofree char *scale = anto_display_format_number(anto_display_member_double(monitor, "scale", 1));
    if (!anto_display_scale_fits_mode(requested, scale)) {
        g_free(scale);
        scale = g_strdup("auto");
        backend_notify("Schermi",
                       "Scala automatica usata per evitare pixel logici frazionari");
    }
    return anto_display_apply_output(context, name, requested, position, scale,
                        anto_display_member_integer(monitor, "transform", 0),
                        anto_display_mirror_for(monitor), error);
}

gboolean anto_display_mutation_position(DisplayContext *context,
                                  json_object *monitors, const char *name,
                                  const char *direction, GError **error) {
    if (!(g_strcmp0(direction, "auto") == 0 || anto_display_valid_direction(direction))) {
        g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
                            "Posizione non valida");
        return FALSE;
    }
    if (!anto_display_require_output(monitors, name, error)) return FALSE;
    json_object *monitor = anto_display_find_output(monitors, name);
    g_autofree char *mode = anto_display_mode_for(monitor);
    g_autofree char *scale = anto_display_format_number(anto_display_member_double(monitor, "scale", 1));
    g_autofree char *position = g_strcmp0(direction, "auto") == 0
        ? g_strdup("auto") : g_strdup_printf("auto-%s", direction);
    return anto_display_apply_output(context, name, mode, position, scale,
                        anto_display_member_integer(monitor, "transform", 0),
                        anto_display_mirror_for(monitor), error);
}
