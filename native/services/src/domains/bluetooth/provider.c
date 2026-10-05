#include "backend.h"
#include "service.h"
#include "local_config.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/file.h>
#include <unistd.h>

typedef struct {
    char *address;
    char *label;
    gboolean is_default;
} BtController;

static BackendCommand run_external(const char *environment,
                                   const char *fallback,
                                   const char *const arguments[],
                                   const char *input) {
    const char *program = backend_program(environment, fallback);
    guint count = 0;
    while (arguments && arguments[count]) count++;
    const char **argv = g_new0(const char *, count + 2);
    argv[0] = program;
    for (guint index = 0; index < count; index++)
        argv[index + 1] = arguments[index];
    BackendCommand result = backend_command_run(argv, input);
    g_free(argv);
    return result;
}

static BackendCommand run_bt(const char *const arguments[]) {
    const char *timeout_program =
        backend_program("ANTO_MENU_TIMEOUT", "timeout");
    const char *bluetoothctl =
        backend_program("ANTO_MENU_BLUETOOTHCTL", "bluetoothctl");
    guint count = 0;
    while (arguments && arguments[count]) count++;
    int requested = 0;
    if (count >= 2 && g_strcmp0(arguments[0], "--timeout") == 0)
        requested = (int)g_ascii_strtoll(arguments[1], NULL, 10);
    g_autofree char *limit = g_strdup_printf(
        "%d", requested > 0 ? requested + 3 :
        (int)g_ascii_strtoll(
            backend_program("ANTO_MENU_BLUETOOTH_READ_TIMEOUT", "5"),
            NULL, 10));
    const char **argv = g_new0(const char *, count + 7);
    guint offset = 0;
    argv[offset++] = timeout_program;
    argv[offset++] = "--foreground";
    argv[offset++] = "--kill-after=1";
    argv[offset++] = limit;
    argv[offset++] = bluetoothctl;
    for (guint index = 0; index < count; index++)
        argv[offset + index] = arguments[index];
    BackendCommand result = backend_command_run(argv, NULL);
    g_free(argv);
    return result;
}

static gboolean bt_failed(const BackendCommand *result) {
    if (!result || result->status != 0) return TRUE;
    g_autofree char *combined = g_ascii_strdown(
        result->stdout_text ? result->stdout_text : "", -1);
    if (result->stderr_text && *result->stderr_text) {
        g_autofree char *error =
            g_ascii_strdown(result->stderr_text, -1);
        g_autofree char *previous = combined;
        combined = g_strconcat(previous, "\n", error, NULL);
    }
    return strstr(combined, "org.bluez.error") ||
           strstr(combined, "invalid command") ||
           strstr(combined, "no default controller") ||
           strstr(combined, "not available") ||
           strstr(combined, "failed");
}

static char *property_value(const char *text, const char *property) {
    g_auto(GStrv) lines = g_strsplit(text ? text : "", "\n", -1);
    for (guint index = 0; lines[index]; index++) {
        char *line = g_strstrip(lines[index]);
        gsize length = strlen(property);
        if (g_ascii_strncasecmp(line, property, length) != 0 ||
            line[length] != ':')
            continue;
        line = g_strstrip(line + length + 1);
        return g_strdup(line);
    }
    return g_strdup("");
}

static gboolean yes_value(const char *value) {
    return g_ascii_strcasecmp(value ? value : "", "yes") == 0 ||
           g_ascii_strcasecmp(value ? value : "", "on") == 0 ||
           g_strcmp0(value, "1") == 0 ||
           g_ascii_strcasecmp(value ? value : "", "true") == 0;
}

static int numeric_value(const char *value, int fallback) {
    if (!value || !*value) return fallback;
    const char *open = strchr(value, '(');
    if (open) {
        char *end = NULL;
        long parsed = strtol(open + 1, &end, 10);
        if (end && end != open + 1 && *end == ')')
            return (int)CLAMP(parsed, G_MININT, G_MAXINT);
    }
    char *end = NULL;
    long parsed = strtol(value, &end, 0);
    return end && end != value
               ? (int)CLAMP(parsed, G_MININT, G_MAXINT)
               : fallback;
}

static void controller_free(gpointer data) {
    BtController *controller = data;
    if (!controller) return;
    g_free(controller->address);
    g_free(controller->label);
    g_free(controller);
}

static GPtrArray *controller_list(void) {
    GPtrArray *controllers =
        g_ptr_array_new_with_free_func(controller_free);
    const char *args[] = {"list", NULL};
    BackendCommand result = run_bt(args);
    g_auto(GStrv) lines = g_strsplit(result.stdout_text, "\n", -1);
    for (guint index = 0; lines[index]; index++) {
        char *line = g_strstrip(lines[index]);
        if (!g_str_has_prefix(line, "Controller ")) continue;
        char *cursor = line + strlen("Controller ");
        char *space = strchr(cursor, ' ');
        if (!space) continue;
        g_autofree char *address = g_strndup(cursor, space - cursor);
        if (!backend_valid_address(address)) continue;
        BtController *controller = g_new0(BtController, 1);
        controller->address = g_ascii_strup(address, -1);
        controller->is_default = strstr(space + 1, " [default]") != NULL;
        if (controller->is_default) {
            char *marker = strstr(space + 1, " [default]");
            controller->label = g_strndup(space + 1, marker - (space + 1));
        } else {
            controller->label = g_strdup(space + 1);
        }
        g_ptr_array_add(controllers, controller);
    }
    backend_command_clear(&result);
    return controllers;
}

static char *selected_controller(GPtrArray *controllers) {
    g_autofree char *saved = NULL;
    saved = anto_local_config_read_text(
        "bluetooth/controller", NULL, NULL);
    if (!saved) {
        g_autofree char *legacy = g_build_filename(
            g_get_user_state_dir(), "anto-menu",
            "bluetooth-controller", NULL);
        g_file_get_contents(legacy, &saved, NULL, NULL);
    }
    if (saved) {
        g_strstrip(saved);
        for (guint index = 0; index < controllers->len; index++) {
            BtController *controller =
                g_ptr_array_index(controllers, index);
            if (g_ascii_strcasecmp(saved, controller->address) == 0)
                return g_strdup(controller->address);
        }
    }
    for (guint index = 0; index < controllers->len; index++) {
        BtController *controller = g_ptr_array_index(controllers, index);
        if (controller->is_default)
            return g_strdup(controller->address);
    }
    if (controllers->len) {
        BtController *first = g_ptr_array_index(controllers, 0);
        return g_strdup(first->address);
    }
    return g_strdup("");
}

static BackendCommand controller_show(const char *address) {
    const char *args[] = {"show", address && *address ? address : NULL, NULL};
    return run_bt(args);
}

static guint device_count(const char *filter) {
    const char *args[] = {
        "devices", filter && *filter ? filter : NULL, NULL,
    };
    BackendCommand result = run_bt(args);
    guint count = 0;
    g_auto(GStrv) lines = g_strsplit(result.stdout_text, "\n", -1);
    for (guint index = 0; lines[index]; index++) {
        char *line = g_strstrip(lines[index]);
        if (!g_str_has_prefix(line, "Device ")) continue;
        char *address = line + strlen("Device ");
        char *space = strchr(address, ' ');
        if (!space) continue;
        *space = '\0';
        if (backend_valid_address(address)) count++;
    }
    backend_command_clear(&result);
    return count;
}

static int print_status(void) {
    g_autoptr(GPtrArray) controllers = controller_list();
    g_autofree char *selected = selected_controller(controllers);
    if (!*selected) {
        puts("STATUS\tno\t-\t-\tno\tno\tno\tno\t0\t0\t0");
        return 0;
    }
    BackendCommand show = controller_show(selected);
    if (bt_failed(&show)) {
        backend_command_clear(&show);
        puts("STATUS\tno\t-\t-\tno\tno\tno\tno\t0\t0\t0");
        return 0;
    }
    g_autofree char *alias = property_value(show.stdout_text, "Alias");
    if (!*alias) {
        g_free(alias);
        alias = property_value(show.stdout_text, "Name");
    }
    g_autofree char *powered =
        property_value(show.stdout_text, "Powered");
    g_autofree char *pairable =
        property_value(show.stdout_text, "Pairable");
    g_autofree char *discoverable =
        property_value(show.stdout_text, "Discoverable");
    g_autofree char *discovering =
        property_value(show.stdout_text, "Discovering");
    g_autofree char *clean_alias = backend_clean_field(
        *alias ? alias : selected);
    g_print("STATUS\tyes\t%s\t%s\t%s\t%s\t%s\t%s\t%u\t%u\t%u\n",
            selected, clean_alias, yes_value(powered) ? "yes" : "no",
            yes_value(pairable) ? "yes" : "no",
            yes_value(discoverable) ? "yes" : "no",
            yes_value(discovering) ? "yes" : "no",
            device_count("Paired"), device_count("Connected"),
            device_count(NULL));
    backend_command_clear(&show);
    return 0;
}

static int print_controllers(const char *only) {
    g_autoptr(GPtrArray) controllers = controller_list();
    g_autofree char *selected = selected_controller(controllers);
    for (guint index = 0; index < controllers->len; index++) {
        BtController *controller = g_ptr_array_index(controllers, index);
        if (only && g_ascii_strcasecmp(only, controller->address) != 0)
            continue;
        BackendCommand show = controller_show(controller->address);
        g_autofree char *alias =
            property_value(show.stdout_text, "Alias");
        g_autofree char *name =
            property_value(show.stdout_text, "Name");
        g_autofree char *powered =
            property_value(show.stdout_text, "Powered");
        g_autofree char *pairable =
            property_value(show.stdout_text, "Pairable");
        g_autofree char *discoverable =
            property_value(show.stdout_text, "Discoverable");
        g_autofree char *discovering =
            property_value(show.stdout_text, "Discovering");
        g_autofree char *safe_alias = backend_clean_field(
            *alias ? alias : controller->label);
        g_autofree char *safe_name = backend_clean_field(
            *name ? name : controller->label);
        g_print("CONTROLLER\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n",
                controller->address, safe_alias, safe_name,
                yes_value(powered) ? "yes" : "no",
                yes_value(pairable) ? "yes" : "no",
                yes_value(discoverable) ? "yes" : "no",
                yes_value(discovering) ? "yes" : "no",
                g_ascii_strcasecmp(selected, controller->address) == 0
                    ? "yes" : "no");
        backend_command_clear(&show);
    }
    return 0;
}

static BackendCommand device_info(const char *address) {
    const char *args[] = {"info", address, NULL};
    return run_bt(args);
}

static int print_device_record(const char *address, const char *fallback) {
    BackendCommand info = device_info(address);
    g_autofree char *name = property_value(info.stdout_text, "Name");
    g_autofree char *alias = property_value(info.stdout_text, "Alias");
    g_autofree char *icon = property_value(info.stdout_text, "Icon");
    g_autofree char *paired = property_value(info.stdout_text, "Paired");
    g_autofree char *trusted = property_value(info.stdout_text, "Trusted");
    g_autofree char *blocked = property_value(info.stdout_text, "Blocked");
    g_autofree char *connected =
        property_value(info.stdout_text, "Connected");
    g_autofree char *battery =
        property_value(info.stdout_text, "Battery Percentage");
    if (!*battery) {
        g_free(battery);
        battery = property_value(info.stdout_text, "Percentage");
    }
    g_autofree char *rssi = property_value(info.stdout_text, "RSSI");
    g_autofree char *safe_name = backend_clean_field(
        *name ? name : (fallback && *fallback ? fallback : address));
    g_autofree char *safe_alias = backend_clean_field(
        *alias ? alias : safe_name);
    g_autofree char *safe_icon =
        backend_clean_field(*icon ? icon : "-");
    int battery_value = numeric_value(battery, -1);
    if (battery_value < 0 || battery_value > 100) battery_value = -1;
    g_print("DEVICE\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%d\t%d\n",
            address, safe_name, safe_alias, safe_icon,
            yes_value(paired) ? "yes" : "no",
            yes_value(trusted) ? "yes" : "no",
            yes_value(blocked) ? "yes" : "no",
            yes_value(connected) ? "yes" : "no",
            battery_value, numeric_value(rssi, -1));
    backend_command_clear(&info);
    return 0;
}

static int print_devices(const char *mode) {
    const char *filter = NULL;
    if (g_strcmp0(mode, "paired") == 0) filter = "Paired";
    else if (g_strcmp0(mode, "connected") == 0) filter = "Connected";
    else if (g_strcmp0(mode, "all") != 0 &&
             g_strcmp0(mode, "discovered") != 0)
        return backend_error(2, "invalid-filter",
                             "Filtro dispositivi non valido");
    const char *args[] = {
        "devices", filter, NULL,
    };
    BackendCommand devices = run_bt(args);
    g_auto(GStrv) lines = g_strsplit(devices.stdout_text ? devices.stdout_text : "", "\n", -1);
    for (guint index = 0; lines && lines[index]; index++) {
        char *line = g_strstrip(lines[index]);
        if (!g_str_has_prefix(line, "Device ")) continue;
        char *address = line + strlen("Device ");
        char *space = strchr(address, ' ');
        if (!space) continue;
        *space = '\0';
        if (!backend_valid_address(address)) continue;
        print_device_record(address, g_strstrip(space + 1));
    }
    backend_command_clear(&devices);
    return 0;
}

static struct json_object *pactl_list(const char *kind) {
    const char *args[] = {"--format=json", "list", kind, NULL};
    BackendCommand result =
        run_external("ANTO_MENU_PACTL", "pactl", args, NULL);
    struct json_object *root = result.status == 0
                                   ? json_tokener_parse(result.stdout_text)
                                   : NULL;
    backend_command_clear(&result);
    if (root && !json_object_is_type(root, json_type_array)) {
        json_object_put(root);
        return NULL;
    }
    return root;
}

static const char *object_string(struct json_object *object,
                                 const char *key) {
    struct json_object *value = NULL;
    return object && json_object_object_get_ex(object, key, &value)
               ? json_object_get_string(value)
               : "";
}

static gboolean card_matches(struct json_object *card,
                             const char *address) {
    g_autofree char *upper = g_ascii_strup(address, -1);
    g_autofree char *token = g_strdup(upper);
    g_strdelimit(token, ":", '_');
    g_autofree char *expected =
        g_strdup_printf("bluez_card.%s", token);
    if (g_strcmp0(object_string(card, "name"), expected) == 0)
        return TRUE;
    struct json_object *properties = NULL;
    if (!json_object_object_get_ex(card, "properties", &properties))
        return FALSE;
    g_autofree char *device = g_ascii_strup(
        object_string(properties, "device.string"), -1);
    return g_strcmp0(device, upper) == 0;
}

static struct json_object *find_audio_object(struct json_object *root,
                                             const char *address) {
    if (!root) return NULL;
    for (guint index = 0; index < json_object_array_length(root); index++) {
        struct json_object *entry =
            json_object_array_get_idx(root, index);
        if (card_matches(entry, address)) return entry;
    }
    return NULL;
}

static int print_audio_profiles(const char *address) {
    struct json_object *root = pactl_list("cards");
    struct json_object *card = find_audio_object(root, address);
    if (!card) {
        if (root) json_object_put(root);
        return 0;
    }
    struct json_object *profiles = NULL;
    json_object_object_get_ex(card, "profiles", &profiles);
    const char *active = object_string(card, "active_profile");
    if (profiles && json_object_is_type(profiles, json_type_object)) {
        json_object_object_foreach(profiles, key, profile) {
            const char *available = object_string(profile, "available");
            const char *description =
                object_string(profile, "description");
            g_autofree char *safe = backend_clean_field(
                *description ? description : key);
            g_print("AUDIO_PROFILE\t%s\t%s\t%s\t%s\t%s\n",
                    object_string(card, "name"), key,
                    *available ? available : "unknown",
                    g_strcmp0(active, key) == 0 ? "yes" : "no", safe);
        }
    }
    json_object_put(root);
    return 0;
}

static int dry_record(const char *action, const char *target,
                      const char *value) {
    g_autofree char *safe_target =
        backend_clean_field(target ? target : "-");
    g_autofree char *safe_value =
        backend_clean_field(value ? value : "-");
    g_print("DRYRUN\t%s\t%s\t%s\n", action, safe_target, safe_value);
    return 0;
}

static int ok_record(const char *action, const char *target,
                     const char *value) {
    g_autofree char *safe_target =
        backend_clean_field(target ? target : "-");
    g_autofree char *safe_value =
        backend_clean_field(value ? value : "-");
    g_print("OK\t%s\t%s\t%s\n", action, safe_target, safe_value);
    return 0;
}

static int select_controller_action(const char *address) {
    if (!backend_valid_address(address))
        return backend_error(2, "invalid-address",
                             "Indirizzo controller non valido");
    g_autoptr(GPtrArray) controllers = controller_list();
    gboolean found = FALSE;
    for (guint index = 0; index < controllers->len; index++) {
        BtController *controller = g_ptr_array_index(controllers, index);
        if (g_ascii_strcasecmp(controller->address, address) == 0) {
            found = TRUE;
            break;
        }
    }
    if (!found && !backend_dry_run())
        return backend_error(1, "no-controller",
                             "Controller Bluetooth non disponibile");
    if (backend_dry_run())
        return dry_record("controller-select", address, "-");
    g_autofree char *upper = g_ascii_strup(address, -1);
    g_autofree char *contents = g_strconcat(upper, "\n", NULL);
    g_autoptr(GError) error = NULL;
    int lock = anto_local_config_lock(
        "bluetooth/controller", TRUE, &error);
    if (lock < 0)
        return backend_error(
            1, "state",
            error ? error->message :
                    "Lock del controller non disponibile");
    gboolean written = anto_local_config_write_text(
        "bluetooth/controller", contents, 0600, &error);
    anto_local_config_unlock(lock);
    if (!written)
        return backend_error(1, "state",
                             error ? error->message :
                                     "Controller selezionato non salvato");
    return ok_record("controller-select", upper, "-");
}

static int audio_profile_action(const char *address, const char *profile) {
    if (!backend_valid_address(address) || !backend_valid_token(profile))
        return backend_error(2, "invalid-profile",
                             "Scheda o profilo Bluetooth non valido");
    if (backend_dry_run())
        return dry_record("audio-profile", address, profile);
    struct json_object *root = pactl_list("cards");
    struct json_object *card = find_audio_object(root, address);
    if (!card) {
        if (root) json_object_put(root);
        return backend_error(1, "no-audio-card",
                             "Scheda audio Bluetooth non disponibile");
    }
    struct json_object *profiles = NULL;
    struct json_object *candidate = NULL;
    json_object_object_get_ex(card, "profiles", &profiles);
    if (!profiles ||
        !json_object_object_get_ex(profiles, profile, &candidate)) {
        json_object_put(root);
        return backend_error(2, "invalid-profile",
                             "Profilo audio non disponibile");
    }
    const char *available = object_string(candidate, "available");
    if (g_strcmp0(available, "no") == 0) {
        json_object_put(root);
        return backend_error(1, "unavailable-profile",
                             "Profilo audio non disponibile");
    }
    g_autofree char *card_name =
        g_strdup(object_string(card, "name"));
    json_object_put(root);
    const char *args[] = {
        "set-card-profile", card_name, profile, NULL,
    };
    BackendCommand result =
        run_external("ANTO_MENU_PACTL", "pactl", args, NULL);
    int status = result.status;
    if (status != 0 && result.stderr_text)
        fputs(result.stderr_text, stderr);
    backend_command_clear(&result);
    return status == 0
               ? ok_record("audio-profile", address, profile)
               : backend_error(1, "pipewire",
                               "Profilo audio non applicato");
}

static struct json_object *find_audio_node(struct json_object *root,
                                          const char *address) {
    if (!root) return NULL;
    g_autofree char *upper = g_ascii_strup(address, -1);
    g_autofree char *token = g_strdup(upper);
    g_strdelimit(token, ":", '_');
    for (guint index = 0; index < json_object_array_length(root); index++) {
        struct json_object *entry =
            json_object_array_get_idx(root, index);
        const char *name = object_string(entry, "name");
        struct json_object *properties = NULL;
        json_object_object_get_ex(entry, "properties", &properties);
        g_autofree char *device = g_ascii_strup(
            object_string(properties, "device.string"), -1);
        if (g_strcmp0(device, upper) == 0 ||
            (name && strstr(name, token)))
            return entry;
    }
    return NULL;
}

static int audio_route_action(const char *address, const char *direction) {
    if (!backend_valid_address(address) ||
        (g_strcmp0(direction, "output") != 0 &&
         g_strcmp0(direction, "input") != 0 &&
         g_strcmp0(direction, "both") != 0))
        return backend_error(2, "invalid-route",
                             "Routing audio Bluetooth non valido");
    if (backend_dry_run())
        return dry_record("audio-route", address, direction);
    gboolean any = FALSE;
    for (guint pass = 0; pass < 2; pass++) {
        gboolean wanted =
            g_strcmp0(direction, "both") == 0 ||
            (pass == 0 && g_strcmp0(direction, "output") == 0) ||
            (pass == 1 && g_strcmp0(direction, "input") == 0);
        if (!wanted) continue;
        const char *kind = pass == 0 ? "sinks" : "sources";
        struct json_object *root = pactl_list(kind);
        struct json_object *node = find_audio_node(root, address);
        if (node) {
            const char *setter =
                pass == 0 ? "set-default-sink" : "set-default-source";
            const char *args[] = {
                setter, object_string(node, "name"), NULL,
            };
            BackendCommand result =
                run_external("ANTO_MENU_PACTL", "pactl", args, NULL);
            if (result.status == 0) any = TRUE;
            backend_command_clear(&result);
        }
        if (root) json_object_put(root);
    }
    return any
               ? ok_record("audio-route", address, direction)
               : backend_error(1, "no-audio-node",
                               "Nodo audio Bluetooth non disponibile");
}

static int mutation_dispatch(int argc, char **argv) {
    int native = backend_bluez_mutation(argc, argv);
    if (native >= 0) return native;
    const char *action = argv[0];
    if (g_strcmp0(action, "controller-select") == 0 && argc == 2)
        return select_controller_action(argv[1]);
    if (g_strcmp0(action, "audio-profile") == 0 && argc == 3)
        return audio_profile_action(argv[1], argv[2]);
    if (g_strcmp0(action, "audio-route") == 0 && argc == 3)
        return audio_route_action(argv[1], argv[2]);
    return backend_usage(
        "bluetooth",
        "{snapshot|controller-select MAC|power STATE|pairable STATE|discoverable STATE [SECONDS]|scan start|stop [SECONDS]|connect|disconnect|pair|trust|untrust|block|unblock|remove MAC|audio-profile MAC PROFILE|audio-route MAC output|input|both}");
}

static int with_lock(int argc, char **argv) {
    if (backend_dry_run()) return mutation_dispatch(argc, argv);
    const char *runtime = g_get_user_runtime_dir();
    if (!runtime || !*runtime) runtime = "/tmp";
    g_autofree char *path =
        g_build_filename(runtime, "anto-menu-bluetooth.lock", NULL);
    int descriptor = open(path, O_CREAT | O_WRONLY, 0600);
    if (descriptor < 0) return mutation_dispatch(argc, argv);
    fcntl(descriptor, F_SETFD, FD_CLOEXEC);
    if (flock(descriptor, LOCK_EX | LOCK_NB) != 0) {
        close(descriptor);
        return backend_error(1, "busy",
                             "Un’altra operazione Bluetooth è in corso");
    }
    int status = mutation_dispatch(argc, argv);
    flock(descriptor, LOCK_UN);
    close(descriptor);
    return status;
}

int anto_bluetooth_execute(int argc, char **argv) {
    (void)argc;

    extern const BackendService anto_service_bluetooth;
    int operation = backend_operation_index(&anto_service_bluetooth, argv[0]);
    if (operation == 0) return backend_bluez_snapshot();
    if (operation == 1) return print_status();
    if (operation == 2) {
        g_autoptr(GPtrArray) controllers = controller_list();
        g_autofree char *selected = selected_controller(controllers);
        return *selected ? print_controllers(selected) : 0;
    }
    if (operation == 3) return print_devices(argc == 2 ? argv[1] : "all");
    if (operation >= 4 && operation <= 6) {
        if (!backend_valid_address(argv[1])) return backend_error(2, "invalid-address", "Indirizzo Bluetooth non valido");
        if (operation == 4) return print_device_record(argv[1], NULL);
        if (operation == 6) return print_audio_profiles(argv[1]);
        BackendCommand info = device_info(argv[1]);
        g_autofree char *value = property_value(info.stdout_text, "Battery Percentage");
        if (!*value) { g_free(value); value = property_value(info.stdout_text, "Percentage"); }
        g_print("%d\n", numeric_value(value, -1));
        backend_command_clear(&info);
        return 0;
    }
    if (g_strcmp0(argv[0], "scan-worker") == 0) return backend_bluez_scan_worker(argv[1], argv[2]);
    return with_lock(argc, argv);
}
