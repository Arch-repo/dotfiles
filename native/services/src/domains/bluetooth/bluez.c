#include "backend.h"
#include "local_config.h"

#include <errno.h>
#include <glib-unix.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* Keep controller selection, device records and counters in one BlueZ
 * ObjectManager reply. bluetoothctl's --timeout keeps successful commands
 * alive for the entire timeout, and its noninteractive mode skips init scripts. */
typedef struct {
    GDBusConnection *connection;
    GVariant *objects;
    char *adapter;
} BluezState;

static void bluez_state_clear(BluezState *state) {
    g_clear_object(&state->connection);
    g_clear_pointer(&state->objects, g_variant_unref);
    g_free(state->adapter);
}

G_DEFINE_AUTO_CLEANUP_CLEAR_FUNC(BluezState, bluez_state_clear)

static GVariant *properties(GVariant *interfaces, const char *interface) {
    return g_variant_lookup_value(interfaces, interface,
                                  G_VARIANT_TYPE("a{sv}"));
}

static const char *string_property(GVariant *props, const char *key) {
    const char *value = NULL;
    if (props) g_variant_lookup(props, key, "&s", &value);
    return value ? value : "";
}

static gboolean boolean_property(GVariant *props, const char *key) {
    gboolean value = FALSE;
    if (props) g_variant_lookup(props, key, "b", &value);
    return value;
}

static const char *boolean_text(GVariant *props, const char *key) {
    return boolean_property(props, key) ? "yes" : "no";
}

static GVariant *object_properties(BluezState *state, const char *path,
                                    const char *interface) {
    g_autoptr(GVariant) interfaces = g_variant_lookup_value(
        state->objects, path, G_VARIANT_TYPE("a{sa{sv}}"));
    return interfaces ? properties(interfaces, interface) : NULL;
}

static gboolean bluez_state_load(BluezState *state, const char *wanted,
                                  GError **error) {
    state->connection = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, error);
    if (!state->connection) return FALSE;
    g_autoptr(GVariant) reply = g_dbus_connection_call_sync(
        state->connection, "org.bluez", "/",
        "org.freedesktop.DBus.ObjectManager", "GetManagedObjects", NULL,
        G_VARIANT_TYPE("(a{oa{sa{sv}}})"), G_DBUS_CALL_FLAGS_NONE,
        5000, NULL, error);
    if (!reply) return FALSE;
    g_variant_get(reply, "(@a{oa{sa{sv}}})", &state->objects);

    g_autofree char *saved = NULL;
    if (!wanted) {
        saved = anto_local_config_read_text("bluetooth/controller", NULL, NULL);
        if (!saved) {
            g_autofree char *legacy = g_build_filename(
                g_get_user_state_dir(), "anto-menu", "bluetooth-controller", NULL);
            g_file_get_contents(legacy, &saved, NULL, NULL);
        }
        if (saved) wanted = g_strstrip(saved);
    }
    GVariantIter iterator;
    const char *path;
    GVariant *interfaces;
    g_variant_iter_init(&iterator, state->objects);
    while (g_variant_iter_next(&iterator, "{&o@a{sa{sv}}}",
                               &path, &interfaces)) {
        g_autoptr(GVariant) owned = interfaces;
        g_autoptr(GVariant) adapter = properties(owned, "org.bluez.Adapter1");
        if (!adapter) continue;
        if (!state->adapter) state->adapter = g_strdup(path);
        if (wanted && g_ascii_strcasecmp(
                wanted, string_property(adapter, "Address")) == 0) {
            g_free(state->adapter);
            state->adapter = g_strdup(path);
            break;
        }
    }
    return TRUE;
}

static int bluez_error(const char *code, GError *error) {
    if (error) g_dbus_error_strip_remote_error(error);
    return backend_error(1, code, error ? error->message :
                         "Operazione Bluetooth non riuscita");
}

static const char *device_adapter(GVariant *device) {
    const char *value = NULL;
    g_variant_lookup(device, "Adapter", "&o", &value);
    return value;
}

static gboolean selected_device(BluezState *state, GVariant *device) {
    return g_strcmp0(device_adapter(device), state->adapter) == 0;
}

static char *find_device(BluezState *state, const char *address) {
    GVariantIter iterator;
    const char *path;
    GVariant *interfaces;
    g_variant_iter_init(&iterator, state->objects);
    while (g_variant_iter_next(&iterator, "{&o@a{sa{sv}}}",
                               &path, &interfaces)) {
        g_autoptr(GVariant) owned = interfaces;
        g_autoptr(GVariant) device = properties(owned, "org.bluez.Device1");
        if (device && selected_device(state, device) &&
            g_ascii_strcasecmp(address, string_property(device, "Address")) == 0)
            return g_strdup(path);
    }
    return NULL;
}

int backend_bluez_snapshot(void) {
    g_auto(BluezState) state = {0};
    g_autoptr(GError) error = NULL;
    if (!bluez_state_load(&state, NULL, &error))
        return bluez_error("snapshot", error);
    if (!state.adapter) {
        puts("STATUS\tno\t-\t-\tno\tno\tno\tno\t0\t0\t0");
        return 0;
    }

    guint paired = 0, connected = 0, seen = 0;
    GVariantIter iterator;
    const char *path;
    GVariant *interfaces;
    g_variant_iter_init(&iterator, state.objects);
    while (g_variant_iter_next(&iterator, "{&o@a{sa{sv}}}",
                               &path, &interfaces)) {
        g_autoptr(GVariant) owned = interfaces;
        g_autoptr(GVariant) device = properties(owned, "org.bluez.Device1");
        if (!device || !selected_device(&state, device)) continue;
        seen++;
        paired += boolean_property(device, "Paired");
        connected += boolean_property(device, "Connected");
    }

    g_autoptr(GVariant) selected = object_properties(
        &state, state.adapter, "org.bluez.Adapter1");
    g_autofree char *alias = backend_clean_field(string_property(selected, "Alias"));
    g_print("STATUS\tyes\t%s\t%s\t%s\t%s\t%s\t%s\t%u\t%u\t%u\n",
            string_property(selected, "Address"), alias,
            boolean_text(selected, "Powered"), boolean_text(selected, "Pairable"),
            boolean_text(selected, "Discoverable"), boolean_text(selected, "Discovering"),
            paired, connected, seen);

    g_variant_iter_init(&iterator, state.objects);
    while (g_variant_iter_next(&iterator, "{&o@a{sa{sv}}}",
                               &path, &interfaces)) {
        g_autoptr(GVariant) owned = interfaces;
        g_autoptr(GVariant) adapter = properties(owned, "org.bluez.Adapter1");
        if (adapter) {
            g_autofree char *safe_alias = backend_clean_field(string_property(adapter, "Alias"));
            g_autofree char *safe_name = backend_clean_field(string_property(adapter, "Name"));
            g_print("CONTROLLER\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n",
                    string_property(adapter, "Address"), safe_alias, safe_name,
                    boolean_text(adapter, "Powered"), boolean_text(adapter, "Pairable"),
                    boolean_text(adapter, "Discoverable"), boolean_text(adapter, "Discovering"),
                    g_strcmp0(path, state.adapter) == 0 ? "yes" : "no");
        }
        g_autoptr(GVariant) device = properties(owned, "org.bluez.Device1");
        if (!device || !selected_device(&state, device)) continue;
        const char *address = string_property(device, "Address");
        const char *name = string_property(device, "Name");
        const char *device_alias = string_property(device, "Alias");
        g_autofree char *safe_name = backend_clean_field(*name ? name : address);
        g_autofree char *safe_alias = backend_clean_field(*device_alias ? device_alias : safe_name);
        g_autofree char *safe_icon = backend_clean_field(string_property(device, "Icon"));
        g_autoptr(GVariant) battery = properties(owned, "org.bluez.Battery1");
        guint8 percentage = 0;
        int battery_value = battery && g_variant_lookup(battery, "Percentage", "y", &percentage)
                            && percentage <= 100 ? percentage : -1;
        gint16 rssi = -1;
        g_variant_lookup(device, "RSSI", "n", &rssi);
        g_print("DEVICE\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%d\t%d\n",
                address, safe_name, safe_alias, *safe_icon ? safe_icon : "-",
                boolean_text(device, "Paired"), boolean_text(device, "Trusted"),
                boolean_text(device, "Blocked"), boolean_text(device, "Connected"),
                battery_value, rssi);
    }
    return 0;
}

static gboolean set_property(BluezState *state, const char *path,
                              const char *interface, const char *property,
                              GVariant *value, GError **error) {
    g_autoptr(GVariant) reply = g_dbus_connection_call_sync(
        state->connection, "org.bluez", path, "org.freedesktop.DBus.Properties",
        "Set", g_variant_new("(ssv)", interface, property, value), NULL,
        G_DBUS_CALL_FLAGS_NONE, 5000, NULL, error);
    return reply != NULL;
}

static gboolean call_method(BluezState *state, const char *path,
                             const char *interface, const char *method,
                             GVariant *arguments, int timeout, GError **error) {
    g_autoptr(GVariant) reply = g_dbus_connection_call_sync(
        state->connection, "org.bluez", path, interface, method, arguments,
        NULL, G_DBUS_CALL_FLAGS_NONE, timeout, NULL, error);
    return reply != NULL;
}

static int record(const char *action, const char *target, const char *value) {
    g_autofree char *safe_target = backend_clean_field(target ? target : "-");
    g_autofree char *safe_value = backend_clean_field(value ? value : "-");
    g_print("%s\t%s\t%s\t%s\n", backend_dry_run() ? "DRYRUN" : "OK",
            action, safe_target, safe_value);
    return 0;
}

static gboolean parse_seconds(const char *text, int minimum, int maximum,
                               int *value) {
    char *end = NULL;
    gint64 parsed = g_ascii_strtoll(text ? text : "", &end, 10);
    if (!end || end == text || *end || parsed < minimum || parsed > maximum)
        return FALSE;
    *value = (int)parsed;
    return TRUE;
}

static char *scan_pid_path(void) {
    return g_build_filename(g_get_user_runtime_dir(), "anto-menu-bluetooth-scan.pid", NULL);
}

static GPid active_scan_pid(const char *file) {
    g_autofree char *text = NULL;
    int value;
    if (!g_file_get_contents(file, &text, NULL, NULL)) return 0;
    g_strstrip(text);
    if (!parse_seconds(text, 2, G_MAXINT, &value)) return 0;
    g_autofree char *path = g_strdup_printf("/proc/%d/cmdline", value);
    g_autofree char *cmdline = NULL;
    gsize size = 0;
    if (!g_file_get_contents(path, &cmdline, &size, NULL) || !size) return 0;
    /* A stale PID must never terminate an unrelated process. */
    gboolean worker = FALSE, bluetoothctl = FALSE, scan = FALSE;
    for (gsize offset = 0; offset < size;) {
        const char *arg = cmdline + offset;
        worker |= g_strcmp0(arg, "scan-worker") == 0;
        scan |= g_strcmp0(arg, "scan") == 0;
        if (!offset) bluetoothctl = g_str_has_suffix(arg, "/bluetoothctl");
        offset += strlen(arg) + 1;
    }
    return worker || (bluetoothctl && scan) ? (GPid)value : 0;
}

static gboolean scan_quit(gpointer data) {
    g_main_loop_quit(data);
    return G_SOURCE_CONTINUE;
}

int backend_bluez_scan_worker(const char *address, const char *duration_text) {
    int duration;
    if (!backend_valid_address(address) || !parse_seconds(duration_text, 5, 180, &duration))
        return backend_error(2, "invalid-scan", "Parametri della scansione non validi");
    g_auto(BluezState) state = {0};
    g_autoptr(GError) error = NULL;
    if (!bluez_state_load(&state, address, &error)) return bluez_error("scan", error);
    g_autoptr(GVariant) adapter = state.adapter
        ? object_properties(&state, state.adapter, "org.bluez.Adapter1") : NULL;
    if (!adapter || g_ascii_strcasecmp(address, string_property(adapter, "Address")) != 0)
        return backend_error(1, "no-controller", "Controller Bluetooth non disponibile");
    if (!call_method(&state, state.adapter, "org.bluez.Adapter1", "StartDiscovery",
                     NULL, 5000, &error))
        return bluez_error("scan", error);
    g_autoptr(GMainLoop) loop = g_main_loop_new(NULL, FALSE);
    guint timer = g_timeout_add_seconds(duration, scan_quit, loop);
    guint terminate = g_unix_signal_add(SIGTERM, scan_quit, loop);
    guint interrupt = g_unix_signal_add(SIGINT, scan_quit, loop);
    puts("READY");
    fflush(stdout);
    g_main_loop_run(loop);
    g_source_remove(timer);
    g_source_remove(terminate);
    g_source_remove(interrupt);
    call_method(&state, state.adapter, "org.bluez.Adapter1", "StopDiscovery",
                 NULL, 5000, NULL);
    return 0;
}

static int scan_action(const char *action, const char *duration_text) {
    if (g_strcmp0(action, "start") != 0 && g_strcmp0(action, "stop") != 0)
        return backend_error(2, "invalid-scan", "Scansione richiesta: start oppure stop");
    int duration = 35;
    if (duration_text && !parse_seconds(duration_text, 5, 180, &duration))
        return backend_error(2, "invalid-duration", "Durata scansione da 5 a 180 secondi");
    if (backend_dry_run()) return record("scan", "-", action);
    g_autofree char *pid_path = scan_pid_path();
    GPid pid = active_scan_pid(pid_path);
    if (g_strcmp0(action, "stop") == 0) {
        if (pid) {
            if (kill(pid, SIGTERM) != 0 && errno != ESRCH)
                return backend_error(1, "scan-stop", "Impossibile terminare la ricerca");
            for (guint attempt = 0; attempt < 100 && active_scan_pid(pid_path); attempt++)
                g_usleep(10000);
            if (active_scan_pid(pid_path))
                return backend_error(1, "scan-stop", "La ricerca si sta ancora arrestando");
        }
        g_unlink(pid_path);
        return record("scan", "-", "stop");
    }
    if (pid) return record("scan", "-", "running");
    g_auto(BluezState) state = {0};
    g_autoptr(GError) error = NULL;
    if (!bluez_state_load(&state, NULL, &error)) return bluez_error("scan", error);
    if (!state.adapter)
        return backend_error(1, "no-controller", "Nessun controller Bluetooth disponibile");
    g_autoptr(GVariant) adapter = object_properties(&state, state.adapter, "org.bluez.Adapter1");
    g_autofree char *executable = g_file_read_link("/proc/self/exe", &error);
    if (!executable) return bluez_error("scan", error);
    g_autofree char *seconds = g_strdup_printf("%d", duration);
    const char *argv[] = {executable, "bluetooth", "scan-worker",
                          string_property(adapter, "Address"), seconds, NULL};
    g_autoptr(GSubprocess) process = g_subprocess_newv(
        argv, G_SUBPROCESS_FLAGS_STDOUT_PIPE | G_SUBPROCESS_FLAGS_STDERR_SILENCE, &error);
    if (!process) return bluez_error("scan", error);
    g_autoptr(GDataInputStream) stream = g_data_input_stream_new(
        g_subprocess_get_stdout_pipe(process));
    g_autofree char *ready = g_data_input_stream_read_line(stream, NULL, NULL, &error);
    if (g_strcmp0(ready, "READY") != 0) {
        g_subprocess_force_exit(process);
        return backend_error(1, "scan", "Ricerca non avviata: controlla radio e adattatore");
    }
    const char *identifier = g_subprocess_get_identifier(process);
    g_autofree char *pid_text = g_strconcat(identifier, "\n", NULL);
    if (!g_file_set_contents(pid_path, pid_text, -1, &error)) {
        g_subprocess_send_signal(process, SIGTERM);
        return bluez_error("scan-pid", error);
    }
    return record("scan", "-", "start");
}

int backend_bluez_mutation(int argc, char **argv) {
    const char *action = argc ? argv[0] : "";
    const char *argument = argc > 1 ? argv[1] : NULL;
    const char *value = argc > 2 ? argv[2] : NULL;
    if (g_strcmp0(action, "scan") == 0 && (argc == 2 || argc == 3))
        return scan_action(argument, value);

    const char *controller_property =
        g_strcmp0(action, "power") == 0 ? "Powered" :
        g_strcmp0(action, "pairable") == 0 ? "Pairable" :
        g_strcmp0(action, "discoverable") == 0 ? "Discoverable" : NULL;
    const char *device_property =
        g_strcmp0(action, "trust") == 0 || g_strcmp0(action, "untrust") == 0 ? "Trusted" :
        g_strcmp0(action, "block") == 0 || g_strcmp0(action, "unblock") == 0 ? "Blocked" : NULL;
    const char *method =
        g_strcmp0(action, "connect") == 0 ? "Connect" :
        g_strcmp0(action, "disconnect") == 0 ? "Disconnect" :
        g_strcmp0(action, "pair") == 0 ? "Pair" :
        g_strcmp0(action, "cancel-pairing") == 0 ? "CancelPairing" :
        g_strcmp0(action, "remove") == 0 ? "RemoveDevice" : NULL;
    gboolean controller_alias = g_strcmp0(action, "controller-alias") == 0;
    gboolean controller_reset = g_strcmp0(action, "controller-alias-reset") == 0;
    gboolean device_alias = g_strcmp0(action, "device-alias") == 0;
    gboolean device_reset = g_strcmp0(action, "device-alias-reset") == 0;
    gboolean device_action = device_property || method || device_alias || device_reset;
    if (!controller_property && !device_action && !controller_alias && !controller_reset)
        return -1;
    int expected = controller_reset ? 1 : device_alias ? 3 : 2;
    if (argc != expected && !(controller_property &&
         g_strcmp0(action, "discoverable") == 0 && argc == 3)) return -1;
    if (device_action && !backend_valid_address(argument))
        return backend_error(2, "invalid-address", "Indirizzo Bluetooth non valido");
    int discoverable_seconds = 180;
    if (controller_property) {
        if (g_strcmp0(argument, "on") != 0 && g_strcmp0(argument, "off") != 0 &&
            g_strcmp0(argument, "toggle") != 0)
            return backend_error(2, "invalid-state", "Stato Bluetooth richiesto: on, off o toggle");
        if (value && !parse_seconds(value, 1, 86400, &discoverable_seconds))
            return backend_error(2, "invalid-duration", "Durata visibilità da 1 a 86400 secondi");
    }
    const char *alias = controller_alias ? argument : value;
    g_autofree char *reset_alias = NULL;
    if ((controller_alias || device_alias) && (!alias || !*alias || strlen(alias) > 128))
        return backend_error(2, "invalid-alias", "Nome Bluetooth non valido");
    if (backend_dry_run() && g_strcmp0(argument, "toggle") != 0)
        return record(action, device_action ? argument : "-",
                       controller_property || controller_alias ? argument : value);

    g_auto(BluezState) state = {0};
    g_autoptr(GError) error = NULL;
    if (!bluez_state_load(&state, NULL, &error)) return bluez_error("bluez", error);
    if (!state.adapter)
        return backend_error(1, "no-controller", "Nessun controller Bluetooth disponibile");
    g_autofree char *device = device_action ? find_device(&state, argument) : NULL;
    if (device_action && !device)
        return backend_error(1, "no-device", "Dispositivo non trovato sul controller selezionato");
    if (controller_property) {
        g_autoptr(GVariant) adapter = object_properties(&state, state.adapter, "org.bluez.Adapter1");
        gboolean enabled = g_strcmp0(argument, "toggle") == 0
            ? !boolean_property(adapter, controller_property) : g_strcmp0(argument, "on") == 0;
        if (backend_dry_run()) return record(action, "-", enabled ? "on" : "off");
        if (g_strcmp0(action, "discoverable") == 0 && value && enabled &&
            !set_property(&state, state.adapter, "org.bluez.Adapter1", "DiscoverableTimeout",
                           g_variant_new_uint32(discoverable_seconds), &error))
            return bluez_error("bluez", error);
        if (!set_property(&state, state.adapter, "org.bluez.Adapter1", controller_property,
                           g_variant_new_boolean(enabled), &error))
            return bluez_error("bluez", error);
        return record(action, "-", enabled ? "on" : "off");
    }
    if (device_property) {
        gboolean enabled = g_strcmp0(action, "trust") == 0 || g_strcmp0(action, "block") == 0;
        if (!set_property(&state, device, "org.bluez.Device1", device_property,
                           g_variant_new_boolean(enabled), &error))
            return bluez_error("bluez", error);
    } else if (method) {
        gboolean remove = g_strcmp0(method, "RemoveDevice") == 0;
        int timeout = 12;
        parse_seconds(backend_program("ANTO_MENU_BLUETOOTH_TIMEOUT", "12"), 1, 180, &timeout);
        if (g_strcmp0(method, "Pair") == 0) timeout = 60;
        if (!call_method(&state, remove ? state.adapter : device,
                          remove ? "org.bluez.Adapter1" : "org.bluez.Device1", method,
                          remove ? g_variant_new("(o)", device) : NULL,
                          timeout * 1000, &error)) {
            if (g_strcmp0(method, "Pair") == 0 &&
                g_error_matches(error, G_IO_ERROR, G_IO_ERROR_TIMED_OUT))
                call_method(&state, device, "org.bluez.Device1", "CancelPairing", NULL, 5000, NULL);
            return bluez_error("bluez", error);
        }
    } else {
        gboolean is_controller = controller_alias || controller_reset;
        g_autoptr(GVariant) props = object_properties(&state,
            is_controller ? state.adapter : device,
            is_controller ? "org.bluez.Adapter1" : "org.bluez.Device1");
        if (controller_reset || device_reset) {
            reset_alias = g_strdup(string_property(props, "Name"));
            alias = reset_alias;
        }
        if (!alias || !*alias)
            return backend_error(1, "no-name", "Nome originale non disponibile");
        if (!set_property(&state, is_controller ? state.adapter : device,
                          is_controller ? "org.bluez.Adapter1" : "org.bluez.Device1",
                          "Alias", g_variant_new_string(alias), &error))
            return bluez_error("bluez", error);
    }
    return record(action, device_action ? argument : "-", alias);
}
