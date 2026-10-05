#include "local_config.h"
#include "virtual_output.h"

#include <errno.h>
#include <fcntl.h>
#include <gio/gio.h>
#include <glib/gstdio.h>
#include <json-c/json.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

#define VIRTUAL_STATE_VERSION 1
#define VIRTUAL_NAME_PREFIX "ANTO-VIRTUAL-"

#ifndef O_CLOEXEC
#define O_CLOEXEC 0
#endif

typedef struct json_object JsonObject;
G_DEFINE_AUTOPTR_CLEANUP_FUNC(JsonObject, json_object_put)

typedef struct {
    int fd;
    char *path;
    gboolean common;
} VirtualLock;

static char *virtual_state_path(void) {
    const char *override = g_getenv("ANTO_VIRTUAL_STATE_FILE");
    if (override && *override) return g_strdup(override);

    return anto_local_config_path("display/virtual-monitors.json", NULL);
}

static gboolean parse_integer(const char *text, int minimum, int maximum,
                              int *result) {
    if (!text || !*text) return FALSE;
    errno = 0;
    char *end = NULL;
    long value = strtol(text, &end, 10);
    if (errno != 0 || !end || *end != '\0' ||
        value < minimum || value > maximum)
        return FALSE;
    *result = (int)value;
    return TRUE;
}

static gboolean parse_number(const char *text, double minimum,
                             double maximum, double *result) {
    if (!text || !*text) return FALSE;
    errno = 0;
    char *end = NULL;
    double value = g_ascii_strtod(text, &end);
    if (errno != 0 || !end || *end != '\0' || !isfinite(value) ||
        value < minimum || value > maximum)
        return FALSE;
    *result = value;
    return TRUE;
}

static gboolean parse_boolean(const char *text, gboolean *result) {
    if (g_strcmp0(text, "true") == 0 ||
        g_strcmp0(text, "1") == 0 ||
        g_strcmp0(text, "persistent") == 0) {
        *result = TRUE;
        return TRUE;
    }
    if (g_strcmp0(text, "false") == 0 ||
        g_strcmp0(text, "0") == 0 ||
        g_strcmp0(text, "temporary") == 0) {
        *result = FALSE;
        return TRUE;
    }
    return FALSE;
}

static gboolean valid_name(const char *name) {
    if (!name || !g_str_has_prefix(name, VIRTUAL_NAME_PREFIX)) return FALSE;
    gsize length = strlen(name);
    if (length <= strlen(VIRTUAL_NAME_PREFIX) || length > 63) return FALSE;
    for (const char *cursor = name + strlen(VIRTUAL_NAME_PREFIX);
         *cursor; cursor++) {
        if (!(g_ascii_isalnum(*cursor) || *cursor == '_' ||
              *cursor == '-' || *cursor == '.'))
            return FALSE;
    }
    return TRUE;
}

static gboolean virtual_output_valid(const MenuVirtualOutput *output) {
    return output && valid_name(output->name) &&
           output->width >= 320 && output->width <= 16384 &&
           output->height >= 320 && output->height <= 16384 &&
           isfinite(output->refresh) &&
           output->refresh >= 24.0 && output->refresh <= 360.0 &&
           isfinite(output->scale) &&
           output->scale >= 0.5 && output->scale <= 4.0 &&
           output->x >= -65535 && output->x <= 65535 &&
           output->y >= -65535 && output->y <= 65535 &&
           output->transform >= 0 && output->transform <= 7;
}

void menu_virtual_output_free(gpointer data) {
    MenuVirtualOutput *output = data;
    if (!output) return;
    g_free(output->name);
    g_free(output);
}

static gboolean json_get_int(struct json_object *object, const char *key,
                             int *value) {
    struct json_object *member = NULL;
    if (!json_object_object_get_ex(object, key, &member) ||
        !json_object_is_type(member, json_type_int))
        return FALSE;
    int64_t parsed = json_object_get_int64(member);
    if (parsed < G_MININT || parsed > G_MAXINT) return FALSE;
    *value = (int)parsed;
    return TRUE;
}

static gboolean json_get_double(struct json_object *object, const char *key,
                                double *value) {
    struct json_object *member = NULL;
    if (!json_object_object_get_ex(object, key, &member) ||
        !(json_object_is_type(member, json_type_double) ||
          json_object_is_type(member, json_type_int)))
        return FALSE;
    double parsed = json_object_get_double(member);
    if (!isfinite(parsed)) return FALSE;
    *value = parsed;
    return TRUE;
}

static gboolean json_get_boolean(struct json_object *object, const char *key,
                                 gboolean *value) {
    struct json_object *member = NULL;
    if (!json_object_object_get_ex(object, key, &member) ||
        !json_object_is_type(member, json_type_boolean))
        return FALSE;
    *value = json_object_get_boolean(member);
    return TRUE;
}

GPtrArray *menu_virtual_output_load(GError **error) {
    g_autofree char *path = virtual_state_path();
    GPtrArray *outputs =
        g_ptr_array_new_with_free_func(menu_virtual_output_free);
    if (!g_file_test(path, G_FILE_TEST_EXISTS)) return outputs;

    g_autofree char *contents = NULL;
    gsize length = 0;
    if (!g_file_get_contents(path, &contents, &length, error)) {
        g_ptr_array_unref(outputs);
        return NULL;
    }

    enum json_tokener_error parse_error = json_tokener_success;
    struct json_object *root =
        json_tokener_parse_verbose(contents, &parse_error);
    if (!root || parse_error != json_tokener_success ||
        !json_object_is_type(root, json_type_object)) {
        if (root) json_object_put(root);
        g_set_error(error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA,
                    "Stato dei monitor virtuali non valido");
        g_ptr_array_unref(outputs);
        return NULL;
    }

    struct json_object *version = NULL;
    struct json_object *array = NULL;
    if (!json_object_object_get_ex(root, "version", &version) ||
        !json_object_is_type(version, json_type_int) ||
        json_object_get_int(version) != VIRTUAL_STATE_VERSION ||
        !json_object_object_get_ex(root, "outputs", &array) ||
        !json_object_is_type(array, json_type_array)) {
        json_object_put(root);
        g_set_error(error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA,
                    "Versione dello stato monitor virtuali non supportata");
        g_ptr_array_unref(outputs);
        return NULL;
    }

    int count = json_object_array_length(array);
    for (int index = 0; index < count; index++) {
        struct json_object *entry = json_object_array_get_idx(array, index);
        struct json_object *name = NULL;
        MenuVirtualOutput *output = g_new0(MenuVirtualOutput, 1);
        gboolean valid =
            entry && json_object_is_type(entry, json_type_object) &&
            json_object_object_get_ex(entry, "name", &name) &&
            json_object_is_type(name, json_type_string) &&
            (output->name = g_strdup(json_object_get_string(name))) &&
            json_get_int(entry, "width", &output->width) &&
            json_get_int(entry, "height", &output->height) &&
            json_get_double(entry, "refresh", &output->refresh) &&
            json_get_double(entry, "scale", &output->scale) &&
            json_get_int(entry, "x", &output->x) &&
            json_get_int(entry, "y", &output->y) &&
            json_get_int(entry, "transform", &output->transform) &&
            json_get_boolean(entry, "persistent", &output->persistent) &&
            virtual_output_valid(output);
        if (!valid) {
            menu_virtual_output_free(output);
            json_object_put(root);
            g_set_error(error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA,
                        "Voce monitor virtuale non valida all’indice %d",
                        index);
            g_ptr_array_unref(outputs);
            return NULL;
        }
        for (guint previous = 0; previous < outputs->len; previous++) {
            MenuVirtualOutput *candidate =
                g_ptr_array_index(outputs, previous);
            if (g_strcmp0(candidate->name, output->name) == 0) {
                menu_virtual_output_free(output);
                json_object_put(root);
                g_set_error(error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA,
                            "Nome monitor virtuale duplicato");
                g_ptr_array_unref(outputs);
                return NULL;
            }
        }
        g_ptr_array_add(outputs, output);
    }
    json_object_put(root);
    return outputs;
}

static struct json_object *outputs_to_json(GPtrArray *outputs) {
    struct json_object *root = json_object_new_object();
    struct json_object *array = json_object_new_array();
    json_object_object_add(
        root, "version", json_object_new_int(VIRTUAL_STATE_VERSION));
    json_object_object_add(root, "outputs", array);
    for (guint index = 0; index < outputs->len; index++) {
        MenuVirtualOutput *output = g_ptr_array_index(outputs, index);
        struct json_object *entry = json_object_new_object();
        json_object_object_add(
            entry, "name", json_object_new_string(output->name));
        json_object_object_add(
            entry, "width", json_object_new_int(output->width));
        json_object_object_add(
            entry, "height", json_object_new_int(output->height));
        json_object_object_add(
            entry, "refresh", json_object_new_double(output->refresh));
        json_object_object_add(
            entry, "scale", json_object_new_double(output->scale));
        json_object_object_add(entry, "x", json_object_new_int(output->x));
        json_object_object_add(entry, "y", json_object_new_int(output->y));
        json_object_object_add(
            entry, "transform", json_object_new_int(output->transform));
        json_object_object_add(
            entry, "persistent",
            json_object_new_boolean(output->persistent));
        json_object_array_add(array, entry);
    }
    return root;
}

static gboolean write_all(int fd, const char *data, gsize length,
                          GError **error) {
    gsize written = 0;
    while (written < length) {
        ssize_t result = write(fd, data + written, length - written);
        if (result < 0 && errno == EINTR) continue;
        if (result <= 0) {
            g_set_error(error, G_FILE_ERROR,
                        g_file_error_from_errno(errno),
                        "Scrittura dello stato fallita: %s",
                        g_strerror(errno));
            return FALSE;
        }
        written += (gsize)result;
    }
    return TRUE;
}

static gboolean atomic_write(const char *path, const char *data, gsize length,
                             GError **error) {
    g_autofree char *directory = g_path_get_dirname(path);
    if (g_mkdir_with_parents(directory, 0700) != 0) {
        g_set_error(error, G_FILE_ERROR, g_file_error_from_errno(errno),
                    "Impossibile creare %s: %s",
                    directory, g_strerror(errno));
        return FALSE;
    }
    g_autofree char *basename = g_path_get_basename(path);
    g_autofree char *template_name =
        g_strdup_printf(".%s.XXXXXX", basename);
    g_autofree char *temporary =
        g_build_filename(directory, template_name, NULL);
    int fd = g_mkstemp_full(temporary, O_WRONLY | O_CLOEXEC, 0600);
    if (fd < 0) {
        g_set_error(error, G_FILE_ERROR, g_file_error_from_errno(errno),
                    "Impossibile creare il file temporaneo: %s",
                    g_strerror(errno));
        return FALSE;
    }

    gboolean success = write_all(fd, data, length, error);
    if (success && fsync(fd) != 0) {
        g_set_error(error, G_FILE_ERROR, g_file_error_from_errno(errno),
                    "Sincronizzazione dello stato fallita: %s",
                    g_strerror(errno));
        success = FALSE;
    }
    if (close(fd) != 0 && success) {
        g_set_error(error, G_FILE_ERROR, g_file_error_from_errno(errno),
                    "Chiusura dello stato fallita: %s",
                    g_strerror(errno));
        success = FALSE;
    }
    if (success && g_rename(temporary, path) != 0) {
        g_set_error(error, G_FILE_ERROR, g_file_error_from_errno(errno),
                    "Commit atomico dello stato fallito: %s",
                    g_strerror(errno));
        success = FALSE;
    }
    if (!success) g_unlink(temporary);
    return success;
}

static gboolean save_outputs(GPtrArray *outputs, GError **error) {
    g_autoptr(JsonObject) root = outputs_to_json(outputs);
    const char *serialized = json_object_to_json_string_ext(
        root, JSON_C_TO_STRING_PRETTY | JSON_C_TO_STRING_SPACED);
    g_autofree char *with_newline = g_strconcat(serialized, "\n", NULL);
    const char *override = g_getenv("ANTO_VIRTUAL_STATE_FILE");
    if (!override || !*override)
        return anto_local_config_write_text(
            "display/virtual-monitors.json", with_newline, 0600, error);
    return atomic_write(override, with_newline, strlen(with_newline), error);
}

static gboolean lock_state(VirtualLock *lock, GError **error) {
    const char *override = g_getenv("ANTO_VIRTUAL_STATE_FILE");
    if (!override || !*override) {
        lock->fd = anto_local_config_lock(
            "display/virtual-monitors", TRUE, error);
        lock->common = lock->fd >= 0;
        return lock->fd >= 0;
    }
    g_autofree char *state = virtual_state_path();
    lock->path = g_strconcat(state, ".lock", NULL);
    g_autofree char *directory = g_path_get_dirname(lock->path);
    if (g_mkdir_with_parents(directory, 0700) != 0) {
        g_set_error(error, G_FILE_ERROR, g_file_error_from_errno(errno),
                    "Impossibile creare la directory dello stato: %s",
                    g_strerror(errno));
        return FALSE;
    }
    lock->fd = g_open(lock->path, O_CREAT | O_RDWR | O_CLOEXEC, 0600);
    if (lock->fd < 0) {
        g_set_error(error, G_FILE_ERROR, g_file_error_from_errno(errno),
                    "Impossibile aprire il lock: %s", g_strerror(errno));
        return FALSE;
    }
    if (flock(lock->fd, LOCK_EX) != 0) {
        g_set_error(error, G_FILE_ERROR, g_file_error_from_errno(errno),
                    "Impossibile bloccare lo stato: %s", g_strerror(errno));
        close(lock->fd);
        lock->fd = -1;
        return FALSE;
    }
    return TRUE;
}

static void unlock_state(VirtualLock *lock) {
    if (!lock) return;
    if (lock->fd >= 0) {
        if (lock->common) {
            anto_local_config_unlock(lock->fd);
        } else {
            (void)flock(lock->fd, LOCK_UN);
            (void)close(lock->fd);
        }
    }
    g_free(lock->path);
    lock->path = NULL;
    lock->fd = -1;
}

static gboolean virtual_dry_run(void) {
    return g_strcmp0(g_getenv("ANTO_VIRTUAL_DRY_RUN"), "1") == 0;
}

static const char *hyprctl_program(void) {
    const char *override = g_getenv("ANTO_VIRTUAL_HYPRCTL");
    return override && *override ? override : "hyprctl";
}

static gboolean run_hyprctl(const char *const arguments[], gboolean mutation,
                            char **stdout_text, GError **error) {
    if (mutation && virtual_dry_run()) {
        GString *line = g_string_new("DRY-RUN");
        g_string_append_printf(line, "\t%s", hyprctl_program());
        for (guint index = 0; arguments[index]; index++)
            g_string_append_printf(line, "\t%s", arguments[index]);
        g_print("%s\n", line->str);
        g_string_free(line, TRUE);
        if (stdout_text) *stdout_text = g_strdup("");
        return TRUE;
    }

    g_autoptr(GPtrArray) argv = g_ptr_array_new();
    g_ptr_array_add(argv, (gpointer)hyprctl_program());
    for (guint index = 0; arguments[index]; index++)
        g_ptr_array_add(argv, (gpointer)arguments[index]);
    g_ptr_array_add(argv, NULL);

    g_autoptr(GSubprocess) process = g_subprocess_newv(
        (const char *const *)argv->pdata,
        G_SUBPROCESS_FLAGS_STDOUT_PIPE | G_SUBPROCESS_FLAGS_STDERR_PIPE,
        error);
    if (!process) return FALSE;

    char *captured_out = NULL;
    char *captured_error = NULL;
    if (!g_subprocess_communicate_utf8(
            process, NULL, NULL, &captured_out, &captured_error, error)) {
        g_free(captured_out);
        g_free(captured_error);
        return FALSE;
    }
    if (!g_subprocess_get_successful(process)) {
        g_set_error(error, G_IO_ERROR, G_IO_ERROR_FAILED,
                    "hyprctl non ha applicato l’operazione: %s",
                    captured_error && *g_strstrip(captured_error)
                        ? captured_error : "errore senza dettagli");
        g_free(captured_out);
        g_free(captured_error);
        return FALSE;
    }
    g_free(captured_error);
    if (stdout_text) *stdout_text = captured_out;
    else g_free(captured_out);
    return TRUE;
}

static struct json_object *parse_monitor_json(const char *text,
                                              GError **error) {
    enum json_tokener_error parse_error = json_tokener_success;
    struct json_object *root =
        json_tokener_parse_verbose(text ? text : "", &parse_error);
    if (!root || parse_error != json_tokener_success ||
        !json_object_is_type(root, json_type_array)) {
        if (root) json_object_put(root);
        g_set_error(error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA,
                    "Risposta monitor di Hyprland non valida");
        return NULL;
    }
    return root;
}

static struct json_object *query_monitors(GError **error) {
    const char *fixture = g_getenv("ANTO_VIRTUAL_MONITORS_JSON");
    if (fixture && *fixture) return parse_monitor_json(fixture, error);

    const char *fixture_file =
        g_getenv("ANTO_VIRTUAL_MONITORS_JSON_FILE");
    if (fixture_file && *fixture_file) {
        g_autofree char *contents = NULL;
        if (!g_file_get_contents(fixture_file, &contents, NULL, error))
            return NULL;
        return parse_monitor_json(contents, error);
    }

    const char *arguments[] = {"monitors", "all", "-j", NULL};
    g_autofree char *output = NULL;
    if (!run_hyprctl(arguments, FALSE, &output, error)) return NULL;
    return parse_monitor_json(output, error);
}

static gboolean monitor_named(struct json_object *monitors,
                              const char *name) {
    if (!monitors) return FALSE;
    int count = json_object_array_length(monitors);
    for (int index = 0; index < count; index++) {
        struct json_object *entry =
            json_object_array_get_idx(monitors, index);
        struct json_object *field = NULL;
        if (entry && json_object_object_get_ex(entry, "name", &field) &&
            json_object_is_type(field, json_type_string) &&
            g_strcmp0(json_object_get_string(field), name) == 0)
            return TRUE;
    }
    return FALSE;
}

static int active_monitor_count(struct json_object *monitors) {
    int active = 0;
    if (!monitors) return active;
    int count = json_object_array_length(monitors);
    for (int index = 0; index < count; index++) {
        struct json_object *entry =
            json_object_array_get_idx(monitors, index);
        struct json_object *disabled = NULL;
        gboolean is_disabled =
            entry && json_object_object_get_ex(entry, "disabled", &disabled)
                ? json_object_get_boolean(disabled) : FALSE;
        if (!is_disabled) active++;
    }
    return active;
}

static void choose_position(struct json_object *monitors, int width,
                            double scale, int *x, int *y) {
    double right_edge = 0.0;
    int count = monitors ? json_object_array_length(monitors) : 0;
    for (int index = 0; index < count; index++) {
        struct json_object *entry =
            json_object_array_get_idx(monitors, index);
        struct json_object *disabled = NULL;
        if (entry && json_object_object_get_ex(
                         entry, "disabled", &disabled) &&
            json_object_get_boolean(disabled))
            continue;
        struct json_object *field_x = NULL;
        struct json_object *field_width = NULL;
        struct json_object *field_scale = NULL;
        double current_x =
            json_object_object_get_ex(entry, "x", &field_x)
                ? json_object_get_double(field_x) : 0.0;
        double current_width =
            json_object_object_get_ex(entry, "width", &field_width)
                ? json_object_get_double(field_width) : 0.0;
        double current_scale =
            json_object_object_get_ex(entry, "scale", &field_scale)
                ? json_object_get_double(field_scale) : 1.0;
        if (current_scale <= 0.0) current_scale = 1.0;
        right_edge = MAX(right_edge, current_x + current_width / current_scale);
    }
    *x = (int)ceil(right_edge);
    *y = 0;
    (void)width;
    (void)scale;
}

static MenuVirtualOutput *find_output(GPtrArray *outputs, const char *name,
                                      guint *index_out) {
    for (guint index = 0; index < outputs->len; index++) {
        MenuVirtualOutput *output = g_ptr_array_index(outputs, index);
        if (g_strcmp0(output->name, name) == 0) {
            if (index_out) *index_out = index;
            return output;
        }
    }
    return NULL;
}

static char *choose_name(GPtrArray *outputs,
                         struct json_object *monitors) {
    for (guint suffix = 1; suffix < 1000; suffix++) {
        g_autofree char *candidate =
            g_strdup_printf(VIRTUAL_NAME_PREFIX "%u", suffix);
        if (!find_output(outputs, candidate, NULL) &&
            !monitor_named(monitors, candidate))
            return g_strdup(candidate);
    }
    return NULL;
}

static gboolean create_live_output(const MenuVirtualOutput *output,
                                   GError **error) {
    const char *create_arguments[] = {
        "output", "create", "headless", output->name, NULL,
    };
    if (!run_hyprctl(create_arguments, TRUE, NULL, error)) return FALSE;

    g_autofree char *rule = g_strdup_printf(
        "%s,%dx%d@%.3f,%dx%d,%.3f,transform,%d",
        output->name, output->width, output->height, output->refresh,
        output->x, output->y, output->scale, output->transform);
    const char *configure_arguments[] = {
        "keyword", "monitor", rule, NULL,
    };
    if (run_hyprctl(configure_arguments, TRUE, NULL, error)) return TRUE;

    const char *rollback_arguments[] = {
        "output", "remove", output->name, NULL,
    };
    g_autoptr(GError) ignored = NULL;
    (void)run_hyprctl(rollback_arguments, TRUE, NULL, &ignored);
    return FALSE;
}

static gboolean configure_live_output(const MenuVirtualOutput *output,
                                      GError **error) {
    g_autofree char *rule = g_strdup_printf(
        "%s,%dx%d@%.3f,%dx%d,%.3f,transform,%d",
        output->name, output->width, output->height, output->refresh,
        output->x, output->y, output->scale, output->transform);
    const char *arguments[] = {"keyword", "monitor", rule, NULL};
    return run_hyprctl(arguments, TRUE, NULL, error);
}

static int print_error(const GError *error) {
    g_printerr("virtual-output: %s\n",
               error ? error->message : "operazione non riuscita");
    return 1;
}

static int action_list(void) {
    g_autoptr(GError) error = NULL;
    g_autoptr(GPtrArray) outputs = menu_virtual_output_load(&error);
    if (!outputs) return print_error(error);

    g_autoptr(JsonObject) monitors = query_monitors(&error);
    if (!monitors) g_clear_error(&error);
    for (guint index = 0; index < outputs->len; index++) {
        MenuVirtualOutput *output = g_ptr_array_index(outputs, index);
        g_print("%s\t%d\t%d\t%.3f\t%.3f\t%d\t%d\t%d\t%s\t%s\n",
                output->name, output->width, output->height,
                output->refresh, output->scale, output->x, output->y,
                output->transform,
                output->persistent ? "persistent" : "temporary",
                monitor_named(monitors, output->name) ? "online" : "offline");
    }
    return 0;
}

static int action_create(int argc, char **argv) {
    if (argc != 8) {
        g_printerr(
            "Uso: anto-menu virtual-output create "
            "{auto|ANTO-VIRTUAL-N} WIDTH HEIGHT HZ SCALE "
            "{persistent|temporary}\n");
        return 2;
    }

    int width = 0;
    int height = 0;
    double refresh = 0.0;
    double scale = 0.0;
    gboolean persistent = FALSE;
    if (!parse_integer(argv[3], 320, 16384, &width) ||
        !parse_integer(argv[4], 320, 16384, &height) ||
        !parse_number(argv[5], 24.0, 360.0, &refresh) ||
        !parse_number(argv[6], 0.5, 4.0, &scale) ||
        !parse_boolean(argv[7], &persistent)) {
        g_printerr("virtual-output: parametri del monitor non validi\n");
        return 2;
    }

    VirtualLock lock = {.fd = -1};
    g_autoptr(GError) error = NULL;
    if (!lock_state(&lock, &error)) return print_error(error);

    g_autoptr(GPtrArray) outputs = menu_virtual_output_load(&error);
    g_autoptr(JsonObject) monitors = NULL;
    if (outputs) monitors = query_monitors(&error);
    if (!outputs || !monitors) {
        int status = print_error(error);
        unlock_state(&lock);
        return status;
    }

    g_autofree char *name =
        g_strcmp0(argv[2], "auto") == 0
            ? choose_name(outputs, monitors) : g_strdup(argv[2]);
    if (!name || !valid_name(name)) {
        g_printerr(
            "virtual-output: il nome deve iniziare con " VIRTUAL_NAME_PREFIX
            " e contenere solo lettere, numeri, punto, trattino o underscore\n");
        unlock_state(&lock);
        return 2;
    }
    if (find_output(outputs, name, NULL) || monitor_named(monitors, name)) {
        g_printerr("virtual-output: %s esiste già\n", name);
        unlock_state(&lock);
        return 1;
    }

    MenuVirtualOutput *output = g_new0(MenuVirtualOutput, 1);
    output->name = g_strdup(name);
    output->width = width;
    output->height = height;
    output->refresh = refresh;
    output->scale = scale;
    output->transform = 0;
    output->persistent = persistent;
    choose_position(monitors, width, scale, &output->x, &output->y);

    if (!create_live_output(output, &error)) {
        menu_virtual_output_free(output);
        int status = print_error(error);
        unlock_state(&lock);
        return status;
    }
    g_ptr_array_add(outputs, output);
    if (!save_outputs(outputs, &error)) {
        const char *rollback[] = {"output", "remove", output->name, NULL};
        g_autoptr(GError) ignored = NULL;
        (void)run_hyprctl(rollback, TRUE, NULL, &ignored);
        int status = print_error(error);
        unlock_state(&lock);
        return status;
    }

    g_print("%s\n", output->name);
    unlock_state(&lock);
    return 0;
}

static int action_remove(const char *name) {
    if (!valid_name(name)) {
        g_printerr("virtual-output: nome non valido\n");
        return 2;
    }

    VirtualLock lock = {.fd = -1};
    g_autoptr(GError) error = NULL;
    if (!lock_state(&lock, &error)) return print_error(error);
    g_autoptr(GPtrArray) outputs = menu_virtual_output_load(&error);
    if (!outputs) {
        int status = print_error(error);
        unlock_state(&lock);
        return status;
    }

    guint index = 0;
    MenuVirtualOutput *output = find_output(outputs, name, &index);
    if (!output) {
        g_printerr("virtual-output: %s non è gestito da Anto Menu\n", name);
        unlock_state(&lock);
        return 1;
    }
    g_autoptr(JsonObject) monitors = query_monitors(&error);
    if (!monitors) {
        int status = print_error(error);
        unlock_state(&lock);
        return status;
    }
    gboolean online = monitor_named(monitors, name);
    if (online && active_monitor_count(monitors) <= 1) {
        g_printerr(
            "virtual-output: rimozione rifiutata, deve restare almeno "
            "un monitor attivo\n");
        unlock_state(&lock);
        return 1;
    }

    MenuVirtualOutput *removed = g_ptr_array_steal_index(outputs, index);
    if (!save_outputs(outputs, &error)) {
        g_ptr_array_insert(outputs, (gint)index, removed);
        int status = print_error(error);
        unlock_state(&lock);
        return status;
    }

    if (online) {
        const char *arguments[] = {"output", "remove", name, NULL};
        if (!run_hyprctl(arguments, TRUE, NULL, &error)) {
            g_ptr_array_insert(outputs, (gint)index, removed);
            g_autoptr(GError) restore_error = NULL;
            (void)save_outputs(outputs, &restore_error);
            int status = print_error(error);
            unlock_state(&lock);
            return status;
        }
    }
    menu_virtual_output_free(removed);
    unlock_state(&lock);
    return 0;
}

static int action_set_persistent(const char *name, const char *value) {
    gboolean persistent = FALSE;
    if (!valid_name(name) || !parse_boolean(value, &persistent)) {
        g_printerr("virtual-output: nome o persistenza non validi\n");
        return 2;
    }

    VirtualLock lock = {.fd = -1};
    g_autoptr(GError) error = NULL;
    if (!lock_state(&lock, &error)) return print_error(error);
    g_autoptr(GPtrArray) outputs = menu_virtual_output_load(&error);
    MenuVirtualOutput *output =
        outputs ? find_output(outputs, name, NULL) : NULL;
    if (!outputs || !output) {
        if (!error)
            g_set_error(&error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND,
                        "Monitor virtuale non trovato");
        int status = print_error(error);
        unlock_state(&lock);
        return status;
    }
    output->persistent = persistent;
    gboolean saved = save_outputs(outputs, &error);
    int status = saved ? 0 : print_error(error);
    unlock_state(&lock);
    return status;
}

static int action_restore(void) {
    VirtualLock lock = {.fd = -1};
    g_autoptr(GError) error = NULL;
    if (!lock_state(&lock, &error)) return print_error(error);
    g_autoptr(GPtrArray) outputs = menu_virtual_output_load(&error);
    if (!outputs) {
        int status = print_error(error);
        unlock_state(&lock);
        return status;
    }

    gboolean pruned = FALSE;
    for (gint index = (gint)outputs->len - 1; index >= 0; index--) {
        MenuVirtualOutput *output =
            g_ptr_array_index(outputs, (guint)index);
        if (!output->persistent) {
            g_ptr_array_remove_index(outputs, (guint)index);
            pruned = TRUE;
        }
    }
    if (pruned && !save_outputs(outputs, &error)) {
        int status = print_error(error);
        unlock_state(&lock);
        return status;
    }
    if (outputs->len == 0) {
        unlock_state(&lock);
        return 0;
    }

    g_autoptr(JsonObject) monitors = NULL;
    for (guint attempt = 0; attempt < 5 && !monitors; attempt++) {
        g_clear_error(&error);
        monitors = query_monitors(&error);
        if (!monitors && attempt < 4) g_usleep(250000);
    }
    if (!monitors) {
        int status = print_error(error);
        unlock_state(&lock);
        return status;
    }

    int failed = 0;
    for (guint index = 0; index < outputs->len; index++) {
        MenuVirtualOutput *output = g_ptr_array_index(outputs, index);
        g_clear_error(&error);
        gboolean success =
            monitor_named(monitors, output->name)
                ? configure_live_output(output, &error)
                : create_live_output(output, &error);
        if (!success) {
            g_printerr("virtual-output: ripristino di %s fallito: %s\n",
                       output->name,
                       error ? error->message : "errore sconosciuto");
            failed++;
        }
    }
    unlock_state(&lock);
    return failed == 0 ? 0 : 1;
}

static gboolean update_from_json(MenuVirtualOutput *output,
                                 struct json_object *entry) {
    struct json_object *field = NULL;
    gboolean changed = FALSE;
    if (json_object_object_get_ex(entry, "x", &field) &&
        json_object_is_type(field, json_type_int)) {
        int value = json_object_get_int(field);
        if (value >= -65535 && value <= 65535 && output->x != value) {
            output->x = value;
            changed = TRUE;
        }
    }
    if (json_object_object_get_ex(entry, "y", &field) &&
        json_object_is_type(field, json_type_int)) {
        int value = json_object_get_int(field);
        if (value >= -65535 && value <= 65535 && output->y != value) {
            output->y = value;
            changed = TRUE;
        }
    }
    if (json_object_object_get_ex(entry, "width", &field) &&
        json_object_is_type(field, json_type_int)) {
        int value = json_object_get_int(field);
        if (value >= 320 && value <= 16384 && output->width != value) {
            output->width = value;
            changed = TRUE;
        }
    }
    if (json_object_object_get_ex(entry, "height", &field) &&
        json_object_is_type(field, json_type_int)) {
        int value = json_object_get_int(field);
        if (value >= 320 && value <= 16384 && output->height != value) {
            output->height = value;
            changed = TRUE;
        }
    }
    if (json_object_object_get_ex(entry, "refreshRate", &field) &&
        (json_object_is_type(field, json_type_double) ||
         json_object_is_type(field, json_type_int))) {
        double value = json_object_get_double(field);
        if (value >= 24.0 && value <= 360.0 &&
            fabs(output->refresh - value) > 0.0005) {
            output->refresh = value;
            changed = TRUE;
        }
    }
    if (json_object_object_get_ex(entry, "scale", &field) &&
        (json_object_is_type(field, json_type_double) ||
         json_object_is_type(field, json_type_int))) {
        double value = json_object_get_double(field);
        if (value >= 0.5 && value <= 4.0 &&
            fabs(output->scale - value) > 0.0005) {
            output->scale = value;
            changed = TRUE;
        }
    }
    if (json_object_object_get_ex(entry, "transform", &field) &&
        json_object_is_type(field, json_type_int)) {
        int value = json_object_get_int(field);
        if (value >= 0 && value <= 7 && output->transform != value) {
            output->transform = value;
            changed = TRUE;
        }
    }
    return changed;
}

static int action_update_layout(const char *layout) {
    g_autoptr(GError) error = NULL;
    g_autoptr(JsonObject) root = parse_monitor_json(layout, &error);
    if (!root) return print_error(error);

    VirtualLock lock = {.fd = -1};
    if (!lock_state(&lock, &error)) return print_error(error);
    g_autoptr(GPtrArray) outputs = menu_virtual_output_load(&error);
    if (!outputs) {
        int status = print_error(error);
        unlock_state(&lock);
        return status;
    }

    gboolean changed = FALSE;
    int count = json_object_array_length(root);
    for (int index = 0; index < count; index++) {
        struct json_object *entry = json_object_array_get_idx(root, index);
        struct json_object *name = NULL;
        if (!entry || !json_object_is_type(entry, json_type_object) ||
            !json_object_object_get_ex(entry, "name", &name) ||
            !json_object_is_type(name, json_type_string))
            continue;
        MenuVirtualOutput *output = find_output(
            outputs, json_object_get_string(name), NULL);
        if (output) changed |= update_from_json(output, entry);
    }
    gboolean saved = !changed || save_outputs(outputs, &error);
    int status = saved ? 0 : print_error(error);
    unlock_state(&lock);
    return status;
}

gboolean menu_virtual_output_is_managed(const char *name) {
    if (!valid_name(name)) return FALSE;
    g_autoptr(GError) error = NULL;
    g_autoptr(GPtrArray) outputs = menu_virtual_output_load(&error);
    return outputs && find_output(outputs, name, NULL) != NULL;
}

int menu_virtual_output_action(int argc, char **argv) {
    if (argc < 1 || g_strcmp0(argv[0], "virtual-output") != 0) return -1;
    if (argc < 2) {
        g_printerr(
            "Uso: anto-menu virtual-output "
            "{list|create|remove|set-persistent|restore|update-layout}\n");
        return 2;
    }
    if (g_strcmp0(argv[1], "list") == 0 && argc == 2)
        return action_list();
    if (g_strcmp0(argv[1], "create") == 0)
        return action_create(argc, argv);
    if (g_strcmp0(argv[1], "remove") == 0 && argc == 3)
        return action_remove(argv[2]);
    if (g_strcmp0(argv[1], "set-persistent") == 0 && argc == 4)
        return action_set_persistent(argv[2], argv[3]);
    if (g_strcmp0(argv[1], "restore") == 0 && argc == 2)
        return action_restore();
    if (g_strcmp0(argv[1], "update-layout") == 0 && argc == 3)
        return action_update_layout(argv[2]);
    g_printerr("virtual-output: comando o numero di argomenti non valido\n");
    return 2;
}
