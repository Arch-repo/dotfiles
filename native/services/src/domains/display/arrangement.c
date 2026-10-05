#include "internal.h"
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

typedef struct {
    char *name;
    gint64 x;
    gint64 y;
    json_object *monitor;
} Placement;


static void placement_free(gpointer data) {
    Placement *placement = data;
    if (!placement) return;
    g_free(placement->name);
    g_free(placement);
}

static gboolean plan_number(json_object *entry, const char *name,
                            gint64 *output) {
    json_object *value = NULL;
    if (!json_object_object_get_ex(entry, name, &value) || !value ||
        !(json_object_is_type(value, json_type_int) ||
          json_object_is_type(value, json_type_double)))
        return FALSE;
    double number = json_object_get_double(value);
    if (!isfinite(number) || floor(number) != number ||
        number < -65535 || number > 65535)
        return FALSE;
    *output = (gint64)number;
    return TRUE;
}

static gboolean placements_overlap(const Placement *left,
                                   const Placement *right) {
    double left_scale = anto_display_member_double(left->monitor, "scale", 1);
    double right_scale = anto_display_member_double(right->monitor, "scale", 1);
    if (left_scale <= 0) left_scale = 1;
    if (right_scale <= 0) right_scale = 1;
    double left_width = anto_display_member_double(left->monitor, "width", 0) / left_scale;
    double left_height = anto_display_member_double(left->monitor, "height", 0) / left_scale;
    double right_width = anto_display_member_double(right->monitor, "width", 0) / right_scale;
    double right_height = anto_display_member_double(right->monitor, "height", 0) / right_scale;
    if ((anto_display_member_integer(left->monitor, "transform", 0) % 2) == 1) {
        double swap = left_width;
        left_width = left_height;
        left_height = swap;
    }
    if ((anto_display_member_integer(right->monitor, "transform", 0) % 2) == 1) {
        double swap = right_width;
        right_width = right_height;
        right_height = swap;
    }
    return (double)left->x < (double)right->x + right_width &&
           (double)left->x + left_width > (double)right->x &&
           (double)left->y < (double)right->y + right_height &&
           (double)left->y + left_height > (double)right->y;
}

static json_object *arranged_snapshot(json_object *monitors,
                                      const char *request, GError **error) {
    json_object *plan = anto_display_parse_json_text(request, error);
    if (!plan) return NULL;
    if (!json_object_is_type(plan, json_type_array) ||
        json_object_array_length(plan) == 0) {
        json_object_put(plan);
        g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA,
                            "Disposizione monitor vuota");
        return NULL;
    }
    GPtrArray *placements = g_ptr_array_new_with_free_func(placement_free);
    GHashTable *names = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
    gint64 minimum_x = G_MAXINT64;
    gint64 minimum_y = G_MAXINT64;
    gboolean valid = TRUE;
    const size_t plan_count = json_object_array_length(plan);
    for (size_t index = 0; index < plan_count; index++) {
        json_object *entry = json_object_array_get_idx(plan, index);
        const char *name = anto_display_member_string(entry, "name", "");
        Placement *placement = g_new0(Placement, 1);
        placement->name = g_strdup(name);
        placement->monitor = anto_display_find_output(monitors, name);
        valid = json_object_is_type(entry, json_type_object) &&
                anto_display_valid_output_name(name) && placement->monitor &&
                !anto_display_member_boolean(placement->monitor, "disabled", FALSE) &&
                g_strcmp0(anto_display_mirror_for(placement->monitor), "none") == 0 &&
                !g_hash_table_contains(names, name) &&
                plan_number(entry, "x", &placement->x) &&
                plan_number(entry, "y", &placement->y);
        if (!valid) {
            placement_free(placement);
            break;
        }
        g_hash_table_add(names, g_strdup(name));
        minimum_x = MIN(minimum_x, placement->x);
        minimum_y = MIN(minimum_y, placement->y);
        g_ptr_array_add(placements, placement);
    }
    guint expected = 0;
    const size_t monitor_count = json_object_array_length(monitors);
    for (size_t index = 0; index < monitor_count; index++) {
        json_object *monitor = json_object_array_get_idx(monitors, index);
        if (!anto_display_member_boolean(monitor, "disabled", FALSE) &&
            g_strcmp0(anto_display_mirror_for(monitor), "none") == 0)
            expected++;
    }
    if (valid && placements->len != expected) valid = FALSE;
    for (guint index = 0; valid && index < placements->len; index++) {
        Placement *placement = g_ptr_array_index(placements, index);
        placement->x -= minimum_x;
        placement->y -= minimum_y;
        if (placement->x < 0 || placement->x > 65535 ||
            placement->y < 0 || placement->y > 65535)
            valid = FALSE;
    }
    for (guint left = 0; valid && left < placements->len; left++)
        for (guint right = left + 1; valid && right < placements->len; right++)
            if (placements_overlap(g_ptr_array_index(placements, left),
                                   g_ptr_array_index(placements, right)))
                valid = FALSE;
    json_object *desired = NULL;
    if (valid) {
        desired = anto_display_snapshot_copy(monitors);
        for (guint index = 0; index < placements->len; index++) {
            Placement *placement = g_ptr_array_index(placements, index);
            json_object *entry = anto_display_find_output(desired, placement->name);
            json_object_object_add(entry, "x", json_object_new_int64(placement->x));
            json_object_object_add(entry, "y", json_object_new_int64(placement->y));
        }
        if (!anto_display_validate_snapshot(desired, error)) {
            json_object_put(desired);
            desired = NULL;
        }
    } else {
        g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA,
                            "Disposizione non valida: usa una volta ogni monitor esteso senza sovrapposizioni");
    }
    g_hash_table_unref(names);
    g_ptr_array_unref(placements);
    json_object_put(plan);
    return desired;
}

static gboolean update_virtual_layout(DisplayContext *context,
                                      json_object *desired, GError **error) {
    if (!g_file_test(context->virtual_state, G_FILE_TEST_EXISTS)) return TRUE;
    GHashTable *names = NULL;
    if (!anto_display_virtual_names(context, &names, error)) return FALSE;
    gboolean has_virtual = g_hash_table_size(names) > 0;
    g_hash_table_unref(names);
    if (!has_virtual) return TRUE;
    const char *serialized = json_object_to_json_string_ext(
        desired, JSON_C_TO_STRING_PLAIN);
    if (context->dry_run) {
        g_print("DRY-RUN virtual-output update-layout %s\n", serialized);
        return TRUE;
    }
    if (!g_file_test(context->virtual_backend, G_FILE_TEST_IS_EXECUTABLE)) {
        g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND,
                            "Backend C dei monitor virtuali non disponibile");
        return FALSE;
    }
    const char *argv[] = {context->virtual_backend, "virtual-output",
                          "update-layout", serialized, NULL};
    BackendCommand command = backend_command_run(argv, NULL);
    gboolean ok = command.status == 0;
    if (!ok)
        g_set_error(error, G_IO_ERROR, G_IO_ERROR_FAILED, "%s",
                    command.stderr_text && *command.stderr_text
                        ? command.stderr_text
                        : "Aggiornamento monitor virtuali non riuscito");
    backend_command_clear(&command);
    return ok;
}

int anto_display_action_arrange(DisplayContext *context, const char *request) {
    g_autoptr(GError) error = NULL;
    if (!anto_display_acquire_lock(context, &error))
        return anto_display_display_error("busy", anto_display_display_error_message(error));
    json_object *before = anto_display_monitor_json(&error);
    if (!before) return anto_display_display_error("state", anto_display_display_error_message(error));
    json_object *rollback = anto_display_snapshot_copy(before);
    json_object *desired = arranged_snapshot(before, request, &error);
    if (!desired) {
        json_object_put(before);
        json_object_put(rollback);
        return anto_display_display_error("layout", anto_display_display_error_message(error));
    }
    context->mutation_applied = FALSE;
    gboolean ok = TRUE;
    const size_t count = json_object_array_length(desired);
    for (size_t index = 0; index < count && ok; index++) {
        json_object *monitor = json_object_array_get_idx(desired, index);
        if (anto_display_member_boolean(monitor, "disabled", FALSE) ||
            g_strcmp0(anto_display_mirror_for(monitor), "none") != 0)
            continue;
        g_autofree char *mode = anto_display_mode_for(monitor);
        g_autofree char *position = anto_display_position_for(monitor);
        g_autofree char *scale = g_strcmp0(mode, "preferred") == 0
                                     ? g_strdup("auto")
                                     : anto_display_format_number(anto_display_member_double(monitor, "scale", 1));
        ok = anto_display_apply_output(context, anto_display_member_string(monitor, "name", ""), mode,
                          position, scale,
                          anto_display_member_integer(monitor, "transform", 0),
                          anto_display_mirror_for(monitor), &error);
    }
    if (ok) ok = update_virtual_layout(context, desired, &error);
    if (ok) ok = anto_display_persist_snapshot(context, desired, &error);
    if (ok && !context->dry_run) {
        g_autoptr(GError) quick_error = NULL;
        if (!anto_display_save_quick_profiles(context, desired, TRUE, &quick_error))
            backend_notify(
                "Schermi",
                "Layout salvato; impossibile aggiornare il profilo rapido");
    }
    if (!ok && context->mutation_applied && !context->dry_run) {
        g_autoptr(GError) restore_error = NULL;
        (void)anto_display_restore_snapshot(context, rollback, &restore_error);
        g_autoptr(GError) virtual_error = NULL;
        (void)update_virtual_layout(context, rollback, &virtual_error);
    }
    json_object_put(before);
    json_object_put(rollback);
    json_object_put(desired);
    if (!ok) return anto_display_display_error("layout", anto_display_display_error_message(error));
    if (!context->dry_run)
        backend_notify("Schermi",
                       "Layout applicato e salvato anche per i prossimi avvii");
    return 0;
}
