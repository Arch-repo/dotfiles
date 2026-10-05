#include "local_config.h"
#include "workspace_output.h"

#include <gio/gio.h>
#include <json-c/json.h>
#include <string.h>

#define WORKSPACE_RULES_RELATIVE "hypr/workspaces.conf"
#define WORKSPACE_SEPARATOR "·"
#define WORKSPACE_SLOT_LIMIT 10

typedef struct {
    char *name;
    char *family;
    char *prefix;
    char *active_workspace;
    gboolean internal;
    gboolean focused;
    int first_slot;
    int slot_count;
    int base_id;
} WorkspaceOutput;

typedef struct json_object JsonObject;
G_DEFINE_AUTOPTR_CLEANUP_FUNC(JsonObject, json_object_put)

static void workspace_output_free(gpointer data) {
    WorkspaceOutput *output = data;
    if (!output) return;
    g_free(output->name);
    g_free(output->family);
    g_free(output->prefix);
    g_free(output->active_workspace);
    g_free(output);
}

static gboolean workspace_dry_run(void) {
    return g_strcmp0(g_getenv("ANTO_WORKSPACE_DRY_RUN"), "1") == 0;
}

static const char *hyprctl_program(void) {
    const char *override = g_getenv("ANTO_WORKSPACE_HYPRCTL");
    return override && *override ? override : "hyprctl";
}

static gboolean hypr_signature_valid(const char *signature) {
    if (!signature || !*signature || strlen(signature) > 255 ||
        g_strcmp0(signature, ".") == 0 ||
        g_strcmp0(signature, "..") == 0)
        return FALSE;
    for (const char *cursor = signature; *cursor; cursor++)
        if (!(g_ascii_isalnum(*cursor) || *cursor == '-' ||
              *cursor == '_' || *cursor == '.'))
            return FALSE;
    return TRUE;
}

static char *hypr_event_socket_path(const char *signature) {
    if (!hypr_signature_valid(signature)) return NULL;
    return g_build_filename(g_get_user_runtime_dir(), "hypr", signature,
                            ".socket2.sock", NULL);
}

static gboolean socket_mtime(const char *path, guint64 *timestamp) {
    if (!path) return FALSE;
    g_autoptr(GFile) file = g_file_new_for_path(path);
    g_autoptr(GFileInfo) info = g_file_query_info(
        file,
        G_FILE_ATTRIBUTE_STANDARD_TYPE ","
        G_FILE_ATTRIBUTE_TIME_MODIFIED ","
        G_FILE_ATTRIBUTE_TIME_MODIFIED_USEC,
        G_FILE_QUERY_INFO_NONE, NULL, NULL);
    if (!info ||
        (g_file_info_get_file_type(info) != G_FILE_TYPE_SPECIAL &&
         !(workspace_dry_run() &&
           g_file_info_get_file_type(info) == G_FILE_TYPE_REGULAR)))
        return FALSE;
    if (g_file_info_get_file_type(info) == G_FILE_TYPE_SPECIAL) {
        g_autoptr(GSocketAddress) address = g_unix_socket_address_new(path);
        g_autoptr(GSocketClient) client = g_socket_client_new();
        g_socket_client_set_timeout(client, 1);
        g_autoptr(GSocketConnection) conn =
            g_socket_client_connect(client, G_SOCKET_CONNECTABLE(address), NULL, NULL);
        if (!conn) return FALSE;
    }
    guint64 seconds = g_file_info_get_attribute_uint64(
        info, G_FILE_ATTRIBUTE_TIME_MODIFIED);
    guint32 microseconds = g_file_info_get_attribute_uint32(
        info, G_FILE_ATTRIBUTE_TIME_MODIFIED_USEC);
    *timestamp = seconds * G_GUINT64_CONSTANT(1000000) + microseconds;
    return TRUE;
}

static void consider_hypr_signature(const char *signature,
                                    char **selected_signature,
                                    guint64 *selected_mtime) {
    g_autofree char *path = hypr_event_socket_path(signature);
    guint64 timestamp = 0;
    if (!path || !socket_mtime(path, &timestamp) ||
        (*selected_signature && timestamp <= *selected_mtime))
        return;
    g_free(*selected_signature);
    *selected_signature = g_strdup(signature);
    *selected_mtime = timestamp;
}

/*
 * A user systemd manager outlives Hyprland.  Its imported environment may
 * therefore contain the signature of a compositor instance that no longer
 * exists.  Select the newest real event socket on every reconnect and update
 * this process before invoking hyprctl.
 */
static char *resolve_hypr_event_socket(void) {
    g_autofree char *hypr_runtime =
        g_build_filename(g_get_user_runtime_dir(), "hypr", NULL);
    g_autoptr(GDir) directory = g_dir_open(hypr_runtime, 0, NULL);
    if (!directory) return NULL;

    char *selected_signature = NULL;
    guint64 selected_mtime = 0;
    const char *imported = g_getenv("HYPRLAND_INSTANCE_SIGNATURE");
    consider_hypr_signature(imported, &selected_signature, &selected_mtime);

    const char *entry = NULL;
    while ((entry = g_dir_read_name(directory)) != NULL)
        consider_hypr_signature(entry, &selected_signature,
                                &selected_mtime);
    if (!selected_signature) return NULL;

    g_setenv("HYPRLAND_INSTANCE_SIGNATURE", selected_signature, TRUE);
    char *socket_path = hypr_event_socket_path(selected_signature);
    g_free(selected_signature);
    return socket_path;
}

static void watcher_retry_pause(guint *seconds) {
    g_usleep((gulong)*seconds * G_USEC_PER_SEC);
    *seconds = MIN(*seconds * 2, 30u);
}

static gboolean run_hyprctl(const char *const arguments[], gboolean mutation,
                            char **stdout_text, GError **error) {
    if (mutation && workspace_dry_run()) {
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
                    "hyprctl non ha completato l’operazione: %s",
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

static JsonObject *parse_monitor_json(const char *text, GError **error) {
    enum json_tokener_error parse_error = json_tokener_success;
    JsonObject *root = json_tokener_parse_verbose(
        text ? text : "", &parse_error);
    if (!root || parse_error != json_tokener_success ||
        !json_object_is_type(root, json_type_array)) {
        if (root) json_object_put(root);
        g_set_error(error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA,
                    "Elenco monitor Hyprland non valido");
        return NULL;
    }
    return root;
}

static JsonObject *query_monitors(GError **error) {
    const char *fixture = g_getenv("ANTO_WORKSPACE_MONITORS_JSON");
    if (fixture && *fixture) return parse_monitor_json(fixture, error);

    const char *fixture_file =
        g_getenv("ANTO_WORKSPACE_MONITORS_JSON_FILE");
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

static gboolean output_name_valid(const char *name) {
    if (!name || !*name || strlen(name) > 63) return FALSE;
    for (const char *cursor = name; *cursor; cursor++)
        if (!(g_ascii_isalnum(*cursor) || *cursor == '-' ||
              *cursor == '_' || *cursor == '.' || *cursor == ':'))
            return FALSE;
    return TRUE;
}

static gboolean output_is_internal(const char *name) {
    return g_str_has_prefix(name, "eDP") ||
           g_str_has_prefix(name, "LVDS") ||
           g_str_has_prefix(name, "DSI");
}

static char *output_family(const char *name) {
    if (output_is_internal(name)) return g_strdup("eDP");
    if (g_str_has_prefix(name, "HDMI")) return g_strdup("HDMI");
    if (g_str_has_prefix(name, "DP")) return g_strdup("DP");
    if (g_str_has_prefix(name, "ANTO-VIRTUAL") ||
        g_str_has_prefix(name, "HEADLESS"))
        return g_strdup("VIRT");

    GString *family = g_string_new(NULL);
    for (const char *cursor = name; *cursor && *cursor != '-'; cursor++)
        if (g_ascii_isalnum(*cursor))
            g_string_append_c(family, *cursor);
    if (family->len == 0) g_string_assign(family, "OUT");
    return g_string_free(family, FALSE);
}

static int trailing_number(const char *name) {
    if (!name || !*name) return -1;
    const char *cursor = name + strlen(name);
    while (cursor > name && g_ascii_isdigit(cursor[-1])) cursor--;
    if (!*cursor) return -1;
    return (int)g_ascii_strtoll(cursor, NULL, 10);
}

static gboolean json_boolean(JsonObject *entry, const char *key,
                             gboolean fallback) {
    JsonObject *field = NULL;
    return json_object_object_get_ex(entry, key, &field)
               ? json_object_get_boolean(field) : fallback;
}

static const char *json_string(JsonObject *entry, const char *key,
                               const char *fallback) {
    JsonObject *field = NULL;
    return json_object_object_get_ex(entry, key, &field) &&
                   json_object_is_type(field, json_type_string)
               ? json_object_get_string(field) : fallback;
}

static GPtrArray *outputs_from_json(JsonObject *root, GError **error) {
    GPtrArray *outputs =
        g_ptr_array_new_with_free_func(workspace_output_free);
    int count = json_object_array_length(root);
    for (int index = 0; index < count; index++) {
        JsonObject *entry = json_object_array_get_idx(root, index);
        if (!entry || !json_object_is_type(entry, json_type_object) ||
            json_boolean(entry, "disabled", FALSE))
            continue;
        const char *mirror = json_string(entry, "mirrorOf", "none");
        if (*mirror && g_strcmp0(mirror, "none") != 0) continue;
        const char *name = json_string(entry, "name", "");
        if (!output_name_valid(name)) {
            g_set_error(error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA,
                        "Nome output non valido nello snapshot");
            g_ptr_array_unref(outputs);
            return NULL;
        }
        WorkspaceOutput *output = g_new0(WorkspaceOutput, 1);
        output->name = g_strdup(name);
        output->family = output_family(name);
        output->internal = output_is_internal(name);
        output->focused = json_boolean(entry, "focused", FALSE);
        JsonObject *active_obj = NULL;
        if (json_object_object_get_ex(entry, "activeWorkspace", &active_obj)) {
            if (json_object_is_type(active_obj, json_type_object)) {
                output->active_workspace =
                    g_strdup(json_string(active_obj, "name", ""));
            } else if (json_object_is_type(active_obj, json_type_string)) {
                output->active_workspace =
                    g_strdup(json_object_get_string(active_obj));
            }
        }
        /* Keep five slots persistent; define names/routing for all ten. */
        output->first_slot = 1;
        output->slot_count = 5;
        g_ptr_array_add(outputs, output);
    }
    if (outputs->len == 0) {
        g_set_error(error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND,
                    "Nessun monitor esteso attivo");
        g_ptr_array_unref(outputs);
        return NULL;
    }

    for (guint index = 0; index < outputs->len; index++) {
        WorkspaceOutput *output = g_ptr_array_index(outputs, index);
        output->base_id = (int)index * 10;
        guint family_count = 0;
        guint family_ordinal = 0;
        for (guint other_index = 0; other_index < outputs->len;
             other_index++) {
            WorkspaceOutput *other =
                g_ptr_array_index(outputs, other_index);
            if (g_strcmp0(output->family, other->family) == 0) {
                family_count++;
                if (other_index <= index) family_ordinal++;
            }
        }
        if (family_count == 1) {
            output->prefix = g_strdup(output->family);
        } else {
            int connector = trailing_number(output->name);
            output->prefix = g_strdup_printf(
                "%s%d", output->family,
                connector >= 0 ? connector : (int)family_ordinal);
        }
    }
    return outputs;
}

static WorkspaceOutput *focused_output(GPtrArray *outputs) {
    for (guint index = 0; index < outputs->len; index++) {
        WorkspaceOutput *output = g_ptr_array_index(outputs, index);
        if (output->focused) return output;
    }
    return outputs->len > 0 ? g_ptr_array_index(outputs, 0) : NULL;
}

static char *workspace_name(const WorkspaceOutput *output, int slot) {
    return g_strdup_printf("%s" WORKSPACE_SEPARATOR "%d",
                           output->prefix, slot);
}

static int workspace_id(const WorkspaceOutput *output, int slot) {
    return output ? output->base_id + slot : slot;
}

static gboolean slot_allowed(const WorkspaceOutput *output, int slot) {
    /*
     * first_slot/slot_count describe only the compact set kept visible by
     * default on Waybar.  Every output can still grow on demand: dispatching
     * an unused slot creates the named Hyprland workspace immediately.
     * The keyboard's 0 key conventionally addresses workspace 10.
     */
    (void)output;
    return slot >= 1 && slot <= WORKSPACE_SLOT_LIMIT;
}

static char *workspace_rule(const WorkspaceOutput *output, int slot) {
    g_autofree char *name = workspace_name(output, slot);
    int id = workspace_id(output, slot);
    return g_strdup_printf(
        "%d, monitor:%s, defaultName:%s, persistent:%s%s",
        id, output->name, name,
        slot < output->first_slot + output->slot_count ? "true" : "false",
        slot == output->first_slot ? ", default:true" : "");
}

static gboolean write_rules(GPtrArray *outputs, GError **error) {
    GString *contents = g_string_new(
        "# Machine-local workspace map generated atomically by Anto.\n"
        "# Every output owns an independent on-demand workspace group.\n"
        "# Slots 1..5 are persistent; slots 6..10 are created on demand.\n");
    for (guint index = 0; index < outputs->len; index++) {
        WorkspaceOutput *output = g_ptr_array_index(outputs, index);
        for (int slot = output->first_slot;
             slot <= WORKSPACE_SLOT_LIMIT; slot++) {
            g_autofree char *rule = workspace_rule(output, slot);
            g_string_append_printf(contents, "workspace = %s\n", rule);
        }
    }
    gboolean written = anto_local_config_write_text(
        WORKSPACE_RULES_RELATIVE, contents->str, 0600, error);
    g_string_free(contents, TRUE);
    return written;
}

static gboolean apply_rules(GPtrArray *outputs, GError **error) {
    for (guint index = 0; index < outputs->len; index++) {
        WorkspaceOutput *output = g_ptr_array_index(outputs, index);
        for (int slot = output->first_slot;
             slot <= WORKSPACE_SLOT_LIMIT; slot++) {
            g_autofree char *rule = workspace_rule(output, slot);
            const char *arguments[] = {"keyword", "workspace", rule, NULL};
            if (!run_hyprctl(arguments, TRUE, NULL, error)) return FALSE;
        }
    }
    return TRUE;
}

static int print_error(const GError *error) {
    g_printerr("workspace-output: %s\n",
               error ? error->message : "operazione non riuscita");
    return 1;
}

static GPtrArray *load_outputs(GError **error) {
    g_autoptr(JsonObject) root = query_monitors(error);
    return root ? outputs_from_json(root, error) : NULL;
}

static int action_sync(gboolean apply) {
    g_autoptr(GError) error = NULL;
    int lock = anto_local_config_lock("hypr/workspaces", TRUE, &error);
    if (lock < 0) return print_error(error);
    g_autoptr(GPtrArray) outputs = load_outputs(&error);
    gboolean success = outputs && write_rules(outputs, &error);
    if (success && apply) success = apply_rules(outputs, &error);
    anto_local_config_unlock(lock);
    return success ? 0 : print_error(error);
}

static int action_list(void) {
    g_autoptr(GError) error = NULL;
    g_autoptr(GPtrArray) outputs = load_outputs(&error);
    if (!outputs) return print_error(error);
    for (guint index = 0; index < outputs->len; index++) {
        WorkspaceOutput *output = g_ptr_array_index(outputs, index);
        for (int slot = output->first_slot;
             slot < output->first_slot + output->slot_count; slot++) {
            g_autofree char *name = workspace_name(output, slot);
            g_print("%s\t%s\t%d\t%s\n",
                    output->name, output->prefix, slot, name);
        }
    }
    return 0;
}

static int parse_slot(const char *text, int *slot) {
    if (!text || !*text) return 0;
    char *end = NULL;
    gint64 value = g_ascii_strtoll(text, &end, 10);
    if (!end || *end != '\0' || value < 0 || value > 99) return 0;
    *slot = (int)value;
    return 1;
}

static int output_active_slot(const WorkspaceOutput *output) {
    if (!output || !output->active_workspace || !*output->active_workspace)
        return 1;
    const char *name = output->active_workspace;
    const char *start = name;
    const char *split = strstr(name, WORKSPACE_SEPARATOR);
    if (split && split != name) {
        start = split + strlen(WORKSPACE_SEPARATOR);
        if (!*start || strstr(start, WORKSPACE_SEPARATOR))
            return 1;
    }
    for (const char *cursor = start; *cursor; ++cursor) {
        if (*cursor < '0' || *cursor > '9')
            return 1;
    }
    if (start[0] == '0')
        return 1;
    char *end = NULL;
    long value = g_ascii_strtoll(start, &end, 10);
    if (!end || *end != '\0' || value < 1)
        return 1;
    int slot = (int)value;
    if (output->base_id > 0 && slot > output->base_id &&
        slot <= output->base_id + 10)
        slot -= output->base_id;
    if (slot < 1 || slot > 10)
        return 1;
    return slot;
}

static int dispatch_to_output(const WorkspaceOutput *output,
                              const char *operation, int slot) {
    if (!output || !slot_allowed(output, slot)) {
        g_printerr(
            "workspace-output: %s usa gli slot da 1 a 10\n",
            output ? output->name : "il monitor");
        return 2;
    }
    int id = workspace_id(output, slot);
    g_autofree char *selector = g_strdup_printf("%d", id);
    const char *dispatcher =
        g_strcmp0(operation, "select") == 0
            ? "workspace" : "movetoworkspace";
    const char *arguments[] = {
        "dispatch", dispatcher, selector, NULL,
    };
    g_autoptr(GError) error = NULL;
    return run_hyprctl(arguments, TRUE, NULL, &error)
               ? 0 : print_error(error);
}

static int action_dispatch(const char *operation, const char *slot_text) {
    int slot = 0;
    if (!parse_slot(slot_text, &slot)) {
        g_printerr("workspace-output: numero workspace non valido\n");
        return 2;
    }
    g_autoptr(GError) error = NULL;
    g_autoptr(GPtrArray) outputs = load_outputs(&error);
    if (!outputs) return print_error(error);
    WorkspaceOutput *output = focused_output(outputs);
    return dispatch_to_output(output, operation, slot);
}

static int action_cycle(const char *operation, int direction) {
    g_autoptr(GError) error = NULL;
    g_autoptr(GPtrArray) outputs = load_outputs(&error);
    if (!outputs) return print_error(error);
    WorkspaceOutput *output = focused_output(outputs);
    if (!output) {
        g_printerr("workspace-output: nessun monitor attivo\n");
        return 2;
    }
    int current_slot = output_active_slot(output);
    int target_slot = CLAMP(current_slot + direction, 1, 10);
    if (target_slot == current_slot) return 0;
    return dispatch_to_output(output, operation, target_slot);
}

static int action_watch(void) {
    guint retry_seconds = 1;
    for (;;) {
        g_autofree char *socket_path = resolve_hypr_event_socket();
        if (!socket_path) {
            watcher_retry_pause(&retry_seconds);
            continue;
        }
        if (action_sync(TRUE) != 0) {
            watcher_retry_pause(&retry_seconds);
            continue;
        }
        if (workspace_dry_run() &&
            g_strcmp0(g_getenv("ANTO_WORKSPACE_WATCH_ONCE"), "1") == 0) {
            g_print("WATCH-SESSION\t%s\n",
                    g_getenv("HYPRLAND_INSTANCE_SIGNATURE"));
            return 0;
        }

        g_autoptr(GSocketClient) client = g_socket_client_new();
        g_autoptr(GSocketAddress) address =
            g_unix_socket_address_new(socket_path);
        g_autoptr(GError) error = NULL;
        g_autoptr(GSocketConnection) connection =
            g_socket_client_connect(
                client, G_SOCKET_CONNECTABLE(address), NULL, &error);
        if (!connection) {
            watcher_retry_pause(&retry_seconds);
            continue;
        }
        retry_seconds = 1;
        gboolean received_event = FALSE;
        g_autoptr(GDataInputStream) stream = g_data_input_stream_new(
            g_io_stream_get_input_stream(G_IO_STREAM(connection)));
        for (;;) {
            gsize length = 0;
            g_autofree char *line = g_data_input_stream_read_line(
                stream, &length, NULL, &error);
            (void)length;
            if (!line) break;
            received_event = TRUE;
            if (g_str_has_prefix(line, "monitoradded") ||
                g_str_has_prefix(line, "monitorremoved")) {
                /* Let Hyprland commit the complete topology before reading it. */
                g_usleep(200 * 1000);
                (void)action_sync(TRUE);
            }
        }
        if (!received_event) watcher_retry_pause(&retry_seconds);
    }
    return 0;
}

int menu_workspace_output_action(int argc, char **argv) {
    if (argc < 1 || g_strcmp0(argv[0], "workspace-output") != 0) return -1;
    if (argc == 2 && g_strcmp0(argv[1], "list") == 0)
        return action_list();
    if (argc == 2 && g_strcmp0(argv[1], "sync") == 0)
        return action_sync(TRUE);
    if (argc == 3 && g_strcmp0(argv[1], "sync") == 0 &&
        g_strcmp0(argv[2], "--write-only") == 0)
        return action_sync(FALSE);
    if (argc == 2 && g_strcmp0(argv[1], "watch") == 0)
        return action_watch();
    if (argc == 3 &&
        (g_strcmp0(argv[1], "select") == 0 ||
         g_strcmp0(argv[1], "move") == 0))
        return action_dispatch(argv[1], argv[2]);
    if (argc == 2 && g_strcmp0(argv[1], "next") == 0)
        return action_cycle("select", 1);
    if (argc == 2 && (g_strcmp0(argv[1], "previous") == 0 ||
                      g_strcmp0(argv[1], "prev") == 0))
        return action_cycle("select", -1);
    if (argc == 2 && (g_strcmp0(argv[1], "move-next") == 0 ||
                      g_strcmp0(argv[1], "shift-next") == 0))
        return action_cycle("move", 1);
    if (argc == 2 && (g_strcmp0(argv[1], "move-previous") == 0 ||
                      g_strcmp0(argv[1], "move-prev") == 0 ||
                      g_strcmp0(argv[1], "shift-previous") == 0 ||
                      g_strcmp0(argv[1], "shift-prev") == 0))
        return action_cycle("move", -1);
    g_printerr(
        "Uso: anto-menu workspace-output "
        "{list|sync [--write-only]|watch|select SLOT|move SLOT|next|previous|move-next|move-previous}\n");
    return 2;
}
