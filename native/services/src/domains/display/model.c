#include "internal.h"
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

const char *anto_display_member_string(json_object *object, const char *name,
                                 const char *fallback) {
    json_object *value = NULL;
    if (!object || !json_object_object_get_ex(object, name, &value) || !value ||
        !json_object_is_type(value, json_type_string))
        return fallback;
    const char *text = json_object_get_string(value);
    return text ? text : fallback;
}

gboolean anto_display_member_boolean(json_object *object, const char *name,
                               gboolean fallback) {
    json_object *value = NULL;
    if (!object || !json_object_object_get_ex(object, name, &value) || !value ||
        !json_object_is_type(value, json_type_boolean))
        return fallback;
    return json_object_get_boolean(value);
}

double anto_display_member_double(json_object *object, const char *name,
                            double fallback) {
    json_object *value = NULL;
    if (!object || !json_object_object_get_ex(object, name, &value) || !value ||
        !(json_object_is_type(value, json_type_double) ||
          json_object_is_type(value, json_type_int)))
        return fallback;
    return json_object_get_double(value);
}

gint64 anto_display_member_integer(json_object *object, const char *name,
                             gint64 fallback) {
    json_object *value = NULL;
    if (!object || !json_object_object_get_ex(object, name, &value) || !value ||
        !json_object_is_type(value, json_type_int))
        return fallback;
    return json_object_get_int64(value);
}

gboolean anto_display_valid_output_name(const char *name) {
    if (!name || !*name || strlen(name) > 128) return FALSE;
    for (const char *cursor = name; *cursor; cursor++)
        if (!(g_ascii_isalnum(*cursor) || strchr("_.:-", *cursor)))
            return FALSE;
    return TRUE;
}

gboolean anto_display_valid_profile_name(const char *name) {
    if (!name || !g_ascii_isalnum(*name) || strlen(name) > 64) return FALSE;
    for (const char *cursor = name + 1; *cursor; cursor++)
        if (!(g_ascii_isalnum(*cursor) || strchr("_.-", *cursor))) return FALSE;
    return TRUE;
}

json_object *anto_display_parse_json_text(const char *text, GError **error) {
    enum json_tokener_error parse_error = json_tokener_success;
    json_object *root = json_tokener_parse_verbose(text ? text : "", &parse_error);
    if (!root || parse_error != json_tokener_success) {
        if (root) json_object_put(root);
        g_set_error(error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA,
                    "JSON monitor non valido: %s",
                    json_tokener_error_desc(parse_error));
        return NULL;
    }
    return root;
}

json_object *anto_display_find_output(json_object *monitors, const char *name) {
    if (!monitors || !anto_display_valid_output_name(name)) return NULL;
    const size_t count = json_object_array_length(monitors);
    for (size_t index = 0; index < count; index++) {
        json_object *monitor = json_object_array_get_idx(monitors, index);
        if (g_strcmp0(anto_display_member_string(monitor, "name", ""), name) == 0)
            return monitor;
    }
    return NULL;
}

gboolean anto_display_require_output(json_object *monitors, const char *name,
                               GError **error) {
    if (!anto_display_valid_output_name(name)) {
        g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
                            "Nome monitor non valido");
        return FALSE;
    }
    if (!anto_display_find_output(monitors, name)) {
        g_set_error(error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND,
                    "Monitor %s non collegato", name);
        return FALSE;
    }
    return TRUE;
}

guint anto_display_active_count(json_object *monitors) {
    guint active = 0;
    const size_t count = json_object_array_length(monitors);
    for (size_t index = 0; index < count; index++)
        if (!anto_display_member_boolean(json_object_array_get_idx(monitors, index),
                            "disabled", FALSE))
            active++;
    return active;
}

static gboolean internal_output(const char *name) {
    return name && (g_str_has_prefix(name, "eDP") ||
                    g_str_has_prefix(name, "LVDS") ||
                    g_str_has_prefix(name, "DSI"));
}

const char *anto_display_primary_output(json_object *monitors) {
    json_object *focused = NULL;
    json_object *first = NULL;
    const size_t count = json_object_array_length(monitors);
    for (size_t index = 0; index < count; index++) {
        json_object *monitor = json_object_array_get_idx(monitors, index);
        if (anto_display_member_boolean(monitor, "disabled", FALSE)) continue;
        if (!first) first = monitor;
        if (internal_output(anto_display_member_string(monitor, "name", "")))
            return anto_display_member_string(monitor, "name", "");
        if (anto_display_member_boolean(monitor, "focused", FALSE)) focused = monitor;
    }
    return anto_display_member_string(focused ? focused : first, "name", "");
}

char *anto_display_format_number(double value) {
    char buffer[G_ASCII_DTOSTR_BUF_SIZE];
    g_ascii_formatd(buffer, sizeof(buffer), "%.3f", value);
    char *end = buffer + strlen(buffer);
    while (end > buffer && end[-1] == '0') *--end = '\0';
    if (end > buffer && end[-1] == '.') *--end = '\0';
    return g_strdup(buffer);
}

char *anto_display_mode_for(json_object *monitor) {
    if (!monitor || anto_display_member_boolean(monitor, "disabled", FALSE))
        return g_strdup("preferred");
    gint64 width = anto_display_member_integer(monitor, "width", 0);
    gint64 height = anto_display_member_integer(monitor, "height", 0);
    if (width <= 0 || height <= 0) return g_strdup("preferred");
    g_autofree char *rate = anto_display_format_number(
        anto_display_member_double(monitor, "refreshRate", 60.0));
    return g_strdup_printf("%" G_GINT64_FORMAT "x%" G_GINT64_FORMAT "@%s",
                           width, height, rate);
}

char *anto_display_position_for(json_object *monitor) {
    if (!monitor || anto_display_member_boolean(monitor, "disabled", FALSE))
        return g_strdup("auto");
    return g_strdup_printf("%" G_GINT64_FORMAT "x%" G_GINT64_FORMAT,
                           anto_display_member_integer(monitor, "x", 0),
                           anto_display_member_integer(monitor, "y", 0));
}

const char *anto_display_mirror_for(json_object *monitor) {
    return anto_display_member_string(monitor, "mirrorOf", "none");
}

json_object *anto_display_snapshot_copy(json_object *monitors) {
    json_object *copy = json_object_new_array();
    static const char *const fields[] = {
        "name", "description", "disabled", "width", "height",
        "refreshRate", "x", "y", "scale", "transform", "mirrorOf",
    };
    const size_t count = json_object_array_length(monitors);
    for (size_t index = 0; index < count; index++) {
        json_object *source = json_object_array_get_idx(monitors, index);
        json_object *entry = json_object_new_object();
        for (guint field = 0; field < G_N_ELEMENTS(fields); field++) {
            json_object *value = NULL;
            if (json_object_object_get_ex(source, fields[field], &value) && value)
                json_object_object_add(entry, fields[field], json_object_get(value));
        }
        json_object_array_add(copy, entry);
    }
    return copy;
}

static gboolean integer_number(json_object *object, const char *name,
                               gint64 minimum, gint64 maximum) {
    json_object *value = NULL;
    if (!json_object_object_get_ex(object, name, &value) || !value) return FALSE;
    if (json_object_is_type(value, json_type_int)) {
        gint64 number = json_object_get_int64(value);
        return number >= minimum && number <= maximum;
    }
    if (!json_object_is_type(value, json_type_double)) return FALSE;
    double number = json_object_get_double(value);
    return isfinite(number) && floor(number) == number &&
           number >= (double)minimum && number <= (double)maximum;
}

static gboolean bounded_number(json_object *object, const char *name,
                               double minimum, double maximum) {
    json_object *value = NULL;
    if (!json_object_object_get_ex(object, name, &value) || !value ||
        !(json_object_is_type(value, json_type_int) ||
          json_object_is_type(value, json_type_double)))
        return FALSE;
    double number = json_object_get_double(value);
    return isfinite(number) && number >= minimum && number <= maximum;
}

gboolean anto_display_validate_snapshot(json_object *monitors, GError **error) {
    if (!monitors || !json_object_is_type(monitors, json_type_array) ||
        json_object_array_length(monitors) == 0) {
        g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA,
                            "Snapshot monitor vuoto");
        return FALSE;
    }
    GHashTable *names = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
    guint active = 0;
    gboolean valid = TRUE;
    const size_t count = json_object_array_length(monitors);
    for (size_t index = 0; index < count && valid; index++) {
        json_object *monitor = json_object_array_get_idx(monitors, index);
        const char *name = anto_display_member_string(monitor, "name", "");
        if (!anto_display_valid_output_name(name) || g_hash_table_contains(names, name)) {
            valid = FALSE;
            break;
        }
        g_hash_table_add(names, g_strdup(name));
        if (anto_display_member_boolean(monitor, "disabled", FALSE)) continue;
        active++;
        valid = integer_number(monitor, "width", 64, 32768) &&
                integer_number(monitor, "height", 64, 32768) &&
                bounded_number(monitor, "refreshRate", 1, 1000) &&
                integer_number(monitor, "x", -65535, 65535) &&
                integer_number(monitor, "y", -65535, 65535) &&
                bounded_number(monitor, "scale", 0.5, 4) &&
                integer_number(monitor, "transform", 0, 7);
    }
    if (active == 0) valid = FALSE;
    for (size_t index = 0; index < count && valid; index++) {
        json_object *monitor = json_object_array_get_idx(monitors, index);
        if (anto_display_member_boolean(monitor, "disabled", FALSE)) continue;
        const char *mirror = anto_display_mirror_for(monitor);
        if (g_strcmp0(mirror, "none") == 0) continue;
        json_object *source = anto_display_find_output(monitors, mirror);
        valid = source && !anto_display_member_boolean(source, "disabled", FALSE) &&
                g_strcmp0(anto_display_mirror_for(source), "none") == 0;
    }
    g_hash_table_unref(names);
    if (!valid)
        g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA,
                            "Snapshot monitor non sicuro");
    return valid;
}

gboolean anto_display_valid_direction(const char *direction) {
    return g_strcmp0(direction, "right") == 0 ||
           g_strcmp0(direction, "left") == 0 ||
           g_strcmp0(direction, "up") == 0 ||
           g_strcmp0(direction, "down") == 0;
}

gboolean anto_display_parse_scale(const char *text, double *scale) {
    if (g_strcmp0(text, "auto") == 0) return TRUE;
    if (!text || !*text) return FALSE;
    char *end = NULL;
    errno = 0;
    double value = g_ascii_strtod(text, &end);
    if (errno || end == text || *end || !isfinite(value) ||
        value < 0.5 || value > 4.0)
        return FALSE;
    if (scale) *scale = value;
    return TRUE;
}

gboolean anto_display_parse_mode(const char *mode, int *width, int *height,
                           double *rate) {
    if (!mode) return FALSE;
    char tail = '\0';
    int parsed = sscanf(mode, "%dx%d@%lf%c", width, height, rate, &tail);
    return parsed == 3 && *width > 0 && *height > 0 && *rate > 0;
}

gboolean anto_display_scale_fits_mode(const char *mode, const char *scale_text) {
    if (g_strcmp0(scale_text, "auto") == 0) return TRUE;
    double scale = 1;
    if (!anto_display_parse_scale(scale_text, &scale)) return FALSE;
    int width = 0;
    int height = 0;
    double rate = 0;
    if (!anto_display_parse_mode(mode, &width, &height, &rate)) return TRUE;
    (void)rate;
    double logical_width = width / scale;
    double logical_height = height / scale;
    return fabs(logical_width - round(logical_width)) < 0.0001 &&
           fabs(logical_height - round(logical_height)) < 0.0001;
}

gboolean anto_display_mode_available(json_object *monitor, const char *requested) {
    json_object *modes = NULL;
    if (!json_object_object_get_ex(monitor, "availableModes", &modes) ||
        !modes || !json_object_is_type(modes, json_type_array))
        return FALSE;
    const size_t count = json_object_array_length(modes);
    for (size_t index = 0; index < count; index++) {
        json_object *entry = json_object_array_get_idx(modes, index);
        if (!entry || !json_object_is_type(entry, json_type_string)) continue;
        g_autofree char *mode = g_strdup(json_object_get_string(entry));
        if (g_str_has_suffix(mode, "Hz")) mode[strlen(mode) - 2] = '\0';
        if (g_strcmp0(mode, requested) == 0) return TRUE;
    }
    return FALSE;
}
