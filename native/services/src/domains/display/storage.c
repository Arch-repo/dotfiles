#include "internal.h"
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

static char *environment_path(const char *name, const char *fallback) {
    const char *value = g_getenv(name);
    return g_strdup(value && *value ? value : fallback);
}

DisplayContext anto_display_display_context(void) {
    DisplayContext context = {.lock_fd = -1};
    const char *state_home = g_get_user_state_dir();
    context.state_root = g_build_filename(state_home, "anto-menu", NULL);

    const char *config_home = g_get_user_config_dir();
    const char *local_override = g_getenv("ANTO_LOCAL_CONFIG_ROOT");
    g_autofree char *local_root = local_override && *local_override
                                      ? g_strdup(local_override)
                                      : g_build_filename(config_home,
                                                         "anto426-local", NULL);
    g_autofree char *profiles = g_build_filename(local_root, "display",
                                                 "profiles", NULL);
    context.profile_dir = environment_path("ANTO_DISPLAY_PROFILE_DIR", profiles);
    g_autofree char *persistent = g_build_filename(local_root, "hypr",
                                                   "monitors.conf", NULL);
    context.persistent_file = environment_path("ANTO_DISPLAY_PERSIST_FILE",
                                               persistent);
    g_autofree char *virtual_state = g_build_filename(
        local_root, "display", "virtual-monitors.json", NULL);
    context.virtual_state = environment_path("ANTO_VIRTUAL_STATE_FILE",
                                             virtual_state);
    g_autofree char *virtual_backend = g_build_filename(
        g_get_home_dir(), ".local", "bin", "anto-menu", NULL);
    context.virtual_backend = environment_path("ANTO_VIRTUAL_BACKEND",
                                               virtual_backend);
    const char *runtime = g_get_user_runtime_dir();
    if (!runtime || !*runtime) runtime = g_get_tmp_dir();
    g_autofree char *lock_name = g_strdup_printf(
        "anto-menu-display-%u.lock", (unsigned)getuid());
    g_autofree char *lock = g_build_filename(runtime, lock_name, NULL);
    context.lock_file = environment_path("ANTO_DISPLAY_LOCK_FILE", lock);
    context.dry_run = g_strcmp0(g_getenv("ANTO_DISPLAY_DRY_RUN"), "1") == 0 ||
                      backend_dry_run();
    context.mock_apply = g_strcmp0(g_getenv("ANTO_DISPLAY_MOCK_APPLY"), "1") == 0;
    return context;
}

void anto_display_display_context_clear(DisplayContext *context) {
    if (!context) return;
    if (context->lock_fd >= 0) close(context->lock_fd);
    g_free(context->state_root);
    g_free(context->profile_dir);
    g_free(context->persistent_file);
    g_free(context->virtual_state);
    g_free(context->virtual_backend);
    g_free(context->lock_file);
    memset(context, 0, sizeof(*context));
    context->lock_fd = -1;
}

gboolean anto_display_acquire_lock(DisplayContext *context, GError **error) {
    if (!context || !context->lock_file || !*context->lock_file) {
        g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
                            "Percorso lock schermi non disponibile");
        return FALSE;
    }
    context->lock_fd = g_open(context->lock_file, O_RDWR | O_CREAT, 0600);
    if (context->lock_fd < 0) {
        g_set_error(error, G_FILE_ERROR, g_file_error_from_errno(errno),
                    "Impossibile aprire il lock schermi: %s", g_strerror(errno));
        return FALSE;
    }
    (void)fcntl(context->lock_fd, F_SETFD, FD_CLOEXEC);
    if (flock(context->lock_fd, LOCK_EX | LOCK_NB) != 0) {
        g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_BUSY,
                            "E' gia' in corso una modifica degli schermi");
        return FALSE;
    }
    return TRUE;
}

json_object *anto_display_load_json_file(const char *path, GError **error) {
    g_autofree char *contents = NULL;
    gsize length = 0;
    if (!g_file_get_contents(path, &contents, &length, error)) return NULL;
    (void)length;
    return anto_display_parse_json_text(contents, error);
}

static gboolean atomic_write(const char *path, const char *contents, gint mode, GError **error) {
    return backend_write_atomic(path, contents, mode, error);
}

static gboolean write_snapshot(const char *path, json_object *monitors,
                               GError **error) {
    json_object *copy = anto_display_snapshot_copy(monitors);
    const char *serialized = json_object_to_json_string_ext(
        copy, JSON_C_TO_STRING_PRETTY | JSON_C_TO_STRING_SPACED);
    g_autofree char *contents = g_strconcat(serialized, "\n", NULL);
    gboolean ok = atomic_write(path, contents, 0600, error);
    json_object_put(copy);
    return ok;
}

gboolean anto_display_virtual_names(DisplayContext *context, GHashTable **names,
                              GError **error) {
    *names = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
    if (!g_file_test(context->virtual_state, G_FILE_TEST_EXISTS)) return TRUE;
    json_object *state = anto_display_load_json_file(context->virtual_state, error);
    if (!state) return FALSE;
    json_object *version = NULL;
    json_object *outputs = NULL;
    gboolean valid = json_object_is_type(state, json_type_object) &&
        json_object_object_get_ex(state, "version", &version) && version &&
        json_object_is_type(version, json_type_int) &&
        json_object_get_int(version) == 1 &&
        json_object_object_get_ex(state, "outputs", &outputs) && outputs &&
        json_object_is_type(outputs, json_type_array);
    if (valid) {
        const size_t count = json_object_array_length(outputs);
        for (size_t index = 0; index < count; index++) {
            json_object *entry = json_object_array_get_idx(outputs, index);
            const char *name = anto_display_member_string(entry, "name", "");
            if (!g_str_has_prefix(name, "ANTO-VIRTUAL-") ||
                !anto_display_valid_output_name(name)) {
                valid = FALSE;
                break;
            }
            g_hash_table_add(*names, g_strdup(name));
        }
    }
    json_object_put(state);
    if (!valid) {
        g_clear_pointer(names, g_hash_table_unref);
        g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA,
                            "Stato dei monitor virtuali non valido");
    }
    return valid;
}

static json_object *physical_snapshot(DisplayContext *context,
                                      json_object *monitors,
                                      GError **error) {
    GHashTable *virtual = NULL;
    if (!anto_display_virtual_names(context, &virtual, error)) return NULL;
    json_object *physical = json_object_new_array();
    const size_t count = json_object_array_length(monitors);
    for (size_t index = 0; index < count; index++) {
        json_object *monitor = json_object_array_get_idx(monitors, index);
        const char *name = anto_display_member_string(monitor, "name", "");
        if (!g_hash_table_contains(virtual, name))
            json_object_array_add(physical, json_object_get(monitor));
    }
    g_hash_table_unref(virtual);
    return physical;
}

static int compare_monitor_names(gconstpointer left, gconstpointer right) {
    json_object *a = *(json_object *const *)left;
    json_object *b = *(json_object *const *)right;
    return g_strcmp0(anto_display_member_string(a, "name", ""),
                     anto_display_member_string(b, "name", ""));
}

static char *render_persistent_config(DisplayContext *context,
                                      json_object *monitors,
                                      GError **error) {
    json_object *physical = physical_snapshot(context, monitors, error);
    if (!physical) return NULL;
    if (!anto_display_validate_snapshot(physical, error)) {
        json_object_put(physical);
        return NULL;
    }
    const size_t count = json_object_array_length(physical);
    GPtrArray *ordered = g_ptr_array_new();
    for (size_t index = 0; index < count; index++)
        g_ptr_array_add(ordered, json_object_array_get_idx(physical, index));
    g_ptr_array_sort(ordered, compare_monitor_names);
    GString *config = g_string_new(
        "# Generated atomically by Anto Display from a validated physical layout.\n"
        "# Virtual/headless outputs are persisted separately by the C backend.\n");
    /* Keep active sources before mirrors, and disabled outputs last. */
    for (guint phase = 0; phase < 3; phase++) {
        for (size_t index = 0; index < count; index++) {
            json_object *monitor = g_ptr_array_index(ordered, (guint)index);
            gboolean disabled = anto_display_member_boolean(monitor, "disabled", FALSE);
            gboolean mirrored = g_strcmp0(anto_display_mirror_for(monitor), "none") != 0;
            guint monitor_phase = disabled ? 2U : mirrored ? 1U : 0U;
            if (monitor_phase != phase) continue;
            const char *name = anto_display_member_string(monitor, "name", "");
            if (disabled) {
                g_string_append_printf(config, "monitor=%s,disable\n", name);
                continue;
            }
            g_autofree char *rate = anto_display_format_number(
                anto_display_member_double(monitor, "refreshRate", 60));
            g_autofree char *scale = anto_display_format_number(
                anto_display_member_double(monitor, "scale", 1));
            g_string_append_printf(
                config, "monitor=%s,%" G_GINT64_FORMAT "x%" G_GINT64_FORMAT
                "@%s,%" G_GINT64_FORMAT "x%" G_GINT64_FORMAT
                ",%s,transform,%" G_GINT64_FORMAT,
                name, anto_display_member_integer(monitor, "width", 0),
                anto_display_member_integer(monitor, "height", 0), rate,
                anto_display_member_integer(monitor, "x", 0),
                anto_display_member_integer(monitor, "y", 0), scale,
                anto_display_member_integer(monitor, "transform", 0));
            if (mirrored)
                g_string_append_printf(config, ",mirror,%s", anto_display_mirror_for(monitor));
            g_string_append_c(config, '\n');
        }
    }
    g_ptr_array_unref(ordered);
    json_object_put(physical);
    return g_string_free(config, FALSE);
}

gboolean anto_display_persist_snapshot(DisplayContext *context,
                                 json_object *monitors, GError **error) {
    g_autofree char *config = render_persistent_config(context, monitors, error);
    if (!config) return FALSE;
    if (context->dry_run) {
        g_print("DRY-RUN persist %s\n%s", context->persistent_file, config);
        return TRUE;
    }
    return atomic_write(context->persistent_file, config, 0600, error);
}

gboolean anto_display_save_quick_profiles(DisplayContext *context,
                                    json_object *monitors,
                                    gboolean both, GError **error) {
    json_object *physical = physical_snapshot(context, monitors, error);
    if (!physical) return FALSE;
    g_autofree char *last = g_build_filename(context->profile_dir, "last.json", NULL);
    gboolean ok = write_snapshot(last, physical, error);
    if (ok && both) {
        g_autofree char *current = g_build_filename(
            context->profile_dir, "current.json", NULL);
        ok = write_snapshot(current, physical, error);
    }
    json_object_put(physical);
    return ok;
}

int anto_display_action_persistent_preview(DisplayContext *context,
                                     json_object *monitors) {
    g_autoptr(GError) error = NULL;
    g_autofree char *config = render_persistent_config(context, monitors, &error);
    if (!config) return anto_display_display_error("snapshot", anto_display_display_error_message(error));
    fputs(config, stdout);
    return 0;
}

int anto_display_action_persist_current(DisplayContext *context,
                                  json_object *monitors) {
    g_autoptr(GError) error = NULL;
    if (!anto_display_acquire_lock(context, &error))
        return anto_display_display_error("busy", anto_display_display_error_message(error));
    if (!anto_display_persist_snapshot(context, monitors, &error))
        return anto_display_display_error("persist", anto_display_display_error_message(error));
    if (!context->dry_run) {
        json_object *physical = physical_snapshot(context, monitors, &error);
        if (!physical) return anto_display_display_error("virtual-state", anto_display_display_error_message(error));
        g_autofree char *current = g_build_filename(
            context->profile_dir, "current.json", NULL);
        gboolean ok = write_snapshot(current, physical, &error);
        json_object_put(physical);
        if (!ok)
            backend_notify(
                "Schermi",
                "Configurazione di avvio salvata; profilo rapido non aggiornato");
        backend_notify("Schermi",
                       "Configurazione corrente salvata anche per il prossimo avvio");
    }
    return 0;
}

int anto_display_action_profile_save(DisplayContext *context, json_object *monitors,
                               const char *name) {
    if (!anto_display_valid_profile_name(name))
        return anto_display_display_error("profile-name", "Nome profilo non valido");
    g_autoptr(GError) error = NULL;
    if (!anto_display_acquire_lock(context, &error))
        return anto_display_display_error("busy", anto_display_display_error_message(error));
    if (context->dry_run) {
        g_print("DRY-RUN profile-save %s\n", name);
        return 0;
    }
    json_object *physical = physical_snapshot(context, monitors, &error);
    if (!physical) return anto_display_display_error("virtual-state", anto_display_display_error_message(error));
    g_autofree char *filename = g_strdup_printf("%s.json", name);
    g_autofree char *path = g_build_filename(context->profile_dir, filename, NULL);
    gboolean ok = write_snapshot(path, physical, &error);
    json_object_put(physical);
    if (!ok) return anto_display_display_error("profile", anto_display_display_error_message(error));
    g_autofree char *message = g_strdup_printf("Profilo “%s” salvato", name);
    backend_notify("Schermi", message);
    return 0;
}

gboolean anto_display_mutation_profile(DisplayContext *context,
                                 json_object *monitors, const char *name,
                                 const char *unused, GError **error) {
    (void)monitors;
    (void)unused;
    if (!anto_display_valid_profile_name(name)) {
        g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
                            "Nome profilo non valido");
        return FALSE;
    }
    g_autofree char *filename = g_strdup_printf("%s.json", name);
    g_autofree char *path = g_build_filename(context->profile_dir, filename, NULL);
    json_object *profile = anto_display_load_json_file(path, error);
    if (!profile) return FALSE;
    gboolean ok = anto_display_restore_snapshot(context, profile, error);
    json_object_put(profile);
    return ok;
}

static int compare_names(gconstpointer left, gconstpointer right) {
    return g_strcmp0(*(char *const *)left, *(char *const *)right);
}

int anto_display_action_profile_list(DisplayContext *context) {
    if (g_mkdir_with_parents(context->profile_dir, 0700) != 0)
        return anto_display_display_error("profile-directory",
                             "Impossibile creare la cartella profili");
    g_autoptr(GError) error = NULL;
    GDir *directory = g_dir_open(context->profile_dir, 0, &error);
    if (!directory) return anto_display_display_error("profile-directory",
                                         anto_display_display_error_message(error));
    GPtrArray *names = g_ptr_array_new_with_free_func(g_free);
    const char *entry = NULL;
    while ((entry = g_dir_read_name(directory)) != NULL) {
        if (!g_str_has_suffix(entry, ".json")) continue;
        g_autofree char *name = g_strndup(entry, strlen(entry) - 5);
        if (g_strcmp0(name, "last") != 0 && anto_display_valid_profile_name(name))
            g_ptr_array_add(names, g_strdup(name));
    }
    g_dir_close(directory);
    g_ptr_array_sort(names, compare_names);
    for (guint index = 0; index < names->len; index++) {
        const char *name = g_ptr_array_index(names, index);
        g_autofree char *filename = g_strdup_printf("%s.json", name);
        g_autofree char *path = g_build_filename(context->profile_dir,
                                                 filename, NULL);
        g_autoptr(GError) read_error = NULL;
        json_object *profile = anto_display_load_json_file(path, &read_error);
        if (!profile || !json_object_is_type(profile, json_type_array)) {
            if (profile) json_object_put(profile);
            continue;
        }
        GString *summary = g_string_new(NULL);
        const size_t count = json_object_array_length(profile);
        for (size_t monitor_index = 0; monitor_index < count; monitor_index++) {
            json_object *monitor = json_object_array_get_idx(profile, monitor_index);
            if (anto_display_member_boolean(monitor, "disabled", FALSE)) continue;
            if (summary->len) g_string_append(summary, " + ");
            g_string_append(summary, anto_display_member_string(monitor, "name", "?"));
        }
        g_print("%s\t%s\n", name,
                summary->len ? summary->str : "profilo salvato");
        g_string_free(summary, TRUE);
        json_object_put(profile);
    }
    g_ptr_array_unref(names);
    return 0;
}

int anto_display_action_profile_delete(DisplayContext *context, const char *name) {
    if (!anto_display_valid_profile_name(name) || g_strcmp0(name, "last") == 0)
        return anto_display_display_error("profile-name", "Nome profilo non valido");
    g_autoptr(GError) error = NULL;
    if (!anto_display_acquire_lock(context, &error))
        return anto_display_display_error("busy", anto_display_display_error_message(error));
    g_autofree char *filename = g_strdup_printf("%s.json", name);
    g_autofree char *path = g_build_filename(context->profile_dir, filename, NULL);
    if (!g_file_test(path, G_FILE_TEST_EXISTS))
        return anto_display_display_error("profile-not-found", "Profilo non trovato");
    if (context->dry_run) {
        g_print("DRY-RUN profile-delete %s\n", name);
        return 0;
    }
    if (g_unlink(path) != 0)
        return anto_display_display_error("profile-delete", g_strerror(errno));
    g_autofree char *message = g_strdup_printf("Profilo “%s” eliminato", name);
    backend_notify("Schermi", message);
    return 0;
}
