#include "backend.h"
#include "service.h"

#include <stdio.h>
#include <string.h>

typedef struct {
    char *ssid;
    char *security;
    char *bssid;
    int signal;
    int frequency;
    int channel;
    gboolean active;
    gboolean saved;
} NetworkRecord;

static BackendCommand run_nm(const char *const arguments[]) {
    const char *program = backend_program("ANTO_MENU_NMCLI", "nmcli");
    guint count = 0;
    while (arguments && arguments[count]) count++;
    gboolean has_wait = count > 0 &&
                        g_strcmp0(arguments[0], "--wait") == 0;
    const char **argv =
        g_new0(const char *, count + (has_wait ? 2 : 4));
    guint offset = 0;
    argv[offset++] = program;
    if (!has_wait) {
        argv[offset++] = "--wait";
        argv[offset++] = backend_program(
            "ANTO_MENU_NMCLI_READ_TIMEOUT", "5");
    }
    for (guint index = 0; index < count; index++)
        argv[offset + index] = arguments[index];
    BackendCommand result = backend_command_run(argv, NULL);
    g_free(argv);
    return result;
}

static GPtrArray *split_terse(const char *line) {
    GPtrArray *fields = g_ptr_array_new_with_free_func(g_free);
    GString *field = g_string_new(NULL);
    gboolean escaped = FALSE;
    for (const char *cursor = line ? line : ""; *cursor; cursor++) {
        if (escaped) {
            g_string_append_c(field, *cursor);
            escaped = FALSE;
        } else if (*cursor == '\\') {
            escaped = TRUE;
        } else if (*cursor == ':') {
            g_ptr_array_add(fields, g_string_free(field, FALSE));
            field = g_string_new(NULL);
        } else {
            g_string_append_c(field, *cursor);
        }
    }
    if (escaped) g_string_append_c(field, '\\');
    g_ptr_array_add(fields, g_string_free(field, FALSE));
    return fields;
}

static const char *field_at(GPtrArray *fields, guint index) {
    return fields && index < fields->len
               ? g_ptr_array_index(fields, index)
               : "";
}

static int parse_integer(const char *value, int fallback) {
    char *end = NULL;
    gint64 parsed = g_ascii_strtoll(value ? value : "", &end, 10);
    if (!end || end == value) return fallback;
    return (int)CLAMP(parsed, G_MININT, G_MAXINT);
}

static void print_transport_field(const char *value) {
    for (const char *cursor = value ? value : ""; *cursor; cursor++) {
        switch (*cursor) {
            case '\\': fputs("\\\\", stdout); break;
            case '\t': fputs("\\t", stdout); break;
            case '\r': fputs("\\r", stdout); break;
            case '\n': fputs("\\n", stdout); break;
            default: fputc(*cursor, stdout); break;
        }
    }
}

static char *first_line(const char *text) {
    const char *end = text ? strchr(text, '\n') : NULL;
    char *line = end ? g_strndup(text, end - text) : g_strdup(text ? text : "");
    g_strstrip(line);
    return line;
}

static char *radio_state(void) {
    const char *args[] = {"-t", "-f", "WIFI", "general", NULL};
    BackendCommand result = run_nm(args);
    char *state = result.status == 0 ? first_line(result.stdout_text) : g_strdup("");
    backend_command_clear(&result);
    return state;
}

static GHashTable *saved_profiles(void) {
    GHashTable *profiles =
        g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
    const char *args[] = {
        "-t", "--escape", "yes", "-f", "NAME,TYPE",
        "connection", "show", NULL,
    };
    BackendCommand result = run_nm(args);
    g_auto(GStrv) lines = g_strsplit(result.stdout_text, "\n", -1);
    for (guint index = 0; lines[index]; index++) {
        g_autoptr(GPtrArray) fields = split_terse(lines[index]);
        if (g_strcmp0(field_at(fields, 1), "802-11-wireless") == 0 &&
            *field_at(fields, 0))
            g_hash_table_add(profiles, g_strdup(field_at(fields, 0)));
    }
    backend_command_clear(&result);
    return profiles;
}

static void network_record_free(gpointer data) {
    NetworkRecord *record = data;
    if (!record) return;
    g_free(record->ssid);
    g_free(record->security);
    g_free(record->bssid);
    g_free(record);
}

static GPtrArray *network_records(GHashTable *profiles) {
    GPtrArray *records =
        g_ptr_array_new_with_free_func(network_record_free);
    GHashTable *by_ssid =
        g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
    const char *args[] = {
        "-t", "--escape", "yes", "-f",
        "IN-USE,SSID,SIGNAL,SECURITY,BSSID,FREQ,CHAN",
        "device", "wifi", "list", "--rescan", "no", NULL,
    };
    BackendCommand result = run_nm(args);
    g_auto(GStrv) lines = g_strsplit(result.stdout_text, "\n", -1);
    for (guint index = 0; lines[index]; index++) {
        if (!*lines[index]) continue;
        g_autoptr(GPtrArray) fields = split_terse(lines[index]);
        const char *ssid = field_at(fields, 1);
        if (!*ssid) continue;
        int signal = CLAMP(parse_integer(field_at(fields, 2), 0), 0, 100);
        gboolean active = g_strcmp0(field_at(fields, 0), "*") == 0;
        NetworkRecord *existing = g_hash_table_lookup(by_ssid, ssid);
        if (existing && (existing->active ||
                         (!active && signal <= existing->signal)))
            continue;
        NetworkRecord *record = existing;
        if (!record) {
            record = g_new0(NetworkRecord, 1);
            record->ssid = g_strdup(ssid);
            g_ptr_array_add(records, record);
            g_hash_table_insert(by_ssid, g_strdup(ssid), record);
        }
        g_free(record->security);
        g_free(record->bssid);
        record->security = g_strdup(field_at(fields, 3));
        record->bssid = g_strdup(field_at(fields, 4));
        record->signal = signal;
        record->active = active;
        record->saved = g_hash_table_contains(profiles, ssid);
        record->frequency = parse_integer(field_at(fields, 5), 0);
        record->channel = parse_integer(field_at(fields, 6), 0);
    }
    backend_command_clear(&result);
    g_hash_table_destroy(by_ssid);
    return records;
}

static char *device_property(const char *device, const char *property) {
    const char *args[] = {
        "-g", property, "device", "show", device, NULL,
    };
    BackendCommand result = run_nm(args);
    char *value =
        result.status == 0 ? first_line(result.stdout_text) : g_strdup("");
    backend_command_clear(&result);
    return value;
}

static char *device_dns(const char *device) {
    const char *args[] = {
        "-g", "IP4.DNS", "device", "show", device, NULL,
    };
    BackendCommand result = run_nm(args);
    GString *joined = g_string_new(NULL);
    g_auto(GStrv) lines = g_strsplit(result.stdout_text, "\n", -1);
    for (guint index = 0; lines[index]; index++) {
        g_strstrip(lines[index]);
        if (!*lines[index]) continue;
        if (joined->len) g_string_append(joined, ", ");
        g_string_append(joined, lines[index]);
    }
    backend_command_clear(&result);
    return g_string_free(joined, FALSE);
}

static int network_snapshot(void) {
    g_autofree char *radio = radio_state();
    gboolean radio_enabled = g_strcmp0(radio, "enabled") == 0;
    g_autofree char *connectivity = NULL;
    {
        const char *args[] = {
            "-t", "-f", "CONNECTIVITY", "general", NULL,
        };
        BackendCommand result = run_nm(args);
        connectivity = result.status == 0
                           ? first_line(result.stdout_text)
                           : g_strdup("unknown");
        backend_command_clear(&result);
    }

    gboolean available = FALSE;
    g_autofree char *device = g_strdup("");
    g_autofree char *state = g_strdup("unavailable");
    g_autofree char *ssid = g_strdup("");
    {
        const char *args[] = {
            "-t", "--escape", "yes", "-f",
            "DEVICE,TYPE,STATE,CONNECTION", "device", "status", NULL,
        };
        BackendCommand result = run_nm(args);
        g_auto(GStrv) lines = g_strsplit(result.stdout_text, "\n", -1);
        for (guint index = 0; lines[index]; index++) {
            g_autoptr(GPtrArray) fields = split_terse(lines[index]);
            if (g_strcmp0(field_at(fields, 1), "wifi") != 0) continue;
            available = TRUE;
            gboolean connected =
                g_strcmp0(field_at(fields, 2), "connected") == 0;
            if (!*device || connected) {
                g_free(device);
                g_free(state);
                g_free(ssid);
                device = g_strdup(field_at(fields, 0));
                state = g_strdup(field_at(fields, 2));
                ssid = g_strcmp0(field_at(fields, 3), "--") == 0
                           ? g_strdup("")
                           : g_strdup(field_at(fields, 3));
            }
            if (connected) break;
        }
        backend_command_clear(&result);
    }

    g_autofree char *ipv4 = g_strdup("");
    g_autofree char *gateway = g_strdup("");
    g_autofree char *dns = g_strdup("");
    if (*device && g_strcmp0(state, "connected") == 0) {
        g_free(ipv4);
        g_free(gateway);
        g_free(dns);
        ipv4 = device_property(device, "IP4.ADDRESS");
        gateway = device_property(device, "IP4.GATEWAY");
        dns = device_dns(device);
    }

    fputs("STATUS\t", stdout);
    fputs(available ? "true\t" : "false\t", stdout);
    fputs(radio_enabled ? "enabled\t" : "disabled\t", stdout);
    print_transport_field(state);
    fputc('\t', stdout);
    print_transport_field(device);
    fputc('\t', stdout);
    print_transport_field(ssid);
    fputc('\t', stdout);
    print_transport_field(ipv4);
    fputc('\t', stdout);
    print_transport_field(gateway);
    fputc('\t', stdout);
    print_transport_field(dns);
    fputc('\t', stdout);
    print_transport_field(*connectivity ? connectivity : "unknown");
    fputc('\n', stdout);

    if (!radio_enabled) return 0;
    g_autoptr(GHashTable) profiles = saved_profiles();
    g_autoptr(GPtrArray) records = network_records(profiles);
    for (guint index = 0; index < records->len; index++) {
        NetworkRecord *record = g_ptr_array_index(records, index);
        fputs("NETWORK\t", stdout);
        print_transport_field(record->ssid);
        g_print("\t%d\t", record->signal);
        print_transport_field(record->security);
        g_print("\t%s\t%s\t", record->active ? "true" : "false",
                record->saved ? "true" : "false");
        print_transport_field(record->bssid);
        g_print("\t%d\t%d\n", record->frequency, record->channel);
    }
    return 0;
}

static int network_status(void) {
    g_autofree char *radio = radio_state();
    if (g_strcmp0(radio, "enabled") != 0) {
        puts("disabled");
        return 0;
    }
    const char *args[] = {
        "-t", "--escape", "yes", "-f",
        "DEVICE,TYPE,STATE,CONNECTION", "device", "status", NULL,
    };
    BackendCommand result = run_nm(args);
    g_auto(GStrv) lines = g_strsplit(result.stdout_text, "\n", -1);
    gboolean printed = FALSE;
    for (guint index = 0; lines[index]; index++) {
        g_autoptr(GPtrArray) fields = split_terse(lines[index]);
        if (g_strcmp0(field_at(fields, 1), "wifi") != 0) continue;
        g_print("%s:%s:%s\n", field_at(fields, 2),
                field_at(fields, 0), field_at(fields, 3));
        printed = TRUE;
        break;
    }
    if (!printed) puts("unavailable::");
    backend_command_clear(&result);
    return 0;
}

static int dry_action(const char *operation, const char *argument) {
    g_print("DRYRUN\tnetwork\t%s\t", operation);
    backend_print_field(argument ? argument : "-");
    fputc('\n', stdout);
    return 0;
}

static int run_action(const char *operation, const char *argument,
                      const char *input) {
    if (backend_dry_run()) return dry_action(operation, argument);
    const char *args[12] = {0};
    guint index = 0;
    if (g_strcmp0(operation, "radio") == 0) {
        args[index++] = "radio";
        args[index++] = "wifi";
        args[index++] = argument;
    } else if (g_strcmp0(operation, "rescan") == 0) {
        args[index++] = "--wait";
        args[index++] = "15";
        args[index++] = "device";
        args[index++] = "wifi";
        args[index++] = "rescan";
    } else if (g_strcmp0(operation, "connect-saved") == 0) {
        args[index++] = "--wait";
        args[index++] = "25";
        args[index++] = "connection";
        args[index++] = "up";
        args[index++] = "id";
        args[index++] = argument;
    } else if (g_strcmp0(operation, "connect-open") == 0 ||
               g_strcmp0(operation, "connect-secure") == 0) {
        args[index++] = "--wait";
        args[index++] = "25";
        args[index++] = "device";
        args[index++] = "wifi";
        args[index++] = "connect";
        args[index++] = argument;
        if (g_strcmp0(operation, "connect-secure") == 0) {
            args[index++] = "password";
            args[index++] = input;
        }
    } else if (g_strcmp0(operation, "disconnect") == 0) {
        args[index++] = "--wait";
        args[index++] = "15";
        args[index++] = "device";
        args[index++] = "disconnect";
        args[index++] = argument;
    } else {
        return backend_error(2, "invalid-action",
                             "Azione Wi-Fi non riconosciuta");
    }
    args[index] = NULL;
    BackendCommand result = run_nm(args);
    if (result.stdout_text) fputs(result.stdout_text, stdout);
    if (result.status != 0 && result.stderr_text)
        fputs(result.stderr_text, stderr);
    int status = result.status;
    backend_command_clear(&result);
    return status;
}

int anto_network_execute(int argc, char **argv) {
    (void)argc;

    extern const BackendService anto_service_network;
    int operation = backend_operation_index(&anto_service_network, argv[0]);
    if (operation == 0) return network_snapshot();
    if (operation == 1) return network_status();
    if (operation == 2) {
        g_autofree char *state = radio_state();
        return run_action("radio", g_strcmp0(state, "enabled") == 0 ? "off" : "on", NULL);
    }
    if (operation == 3 && g_strcmp0(argv[1], "on") != 0 && g_strcmp0(argv[1], "off") != 0)
        return backend_error(2, "invalid-state", "Stato radio richiesto: on oppure off");
    if (operation == 4) return run_action("rescan", NULL, NULL);
    if (!argv[1] || !*argv[1]) return backend_error(2, "missing-target", "Destinazione Wi-Fi mancante");
    if (operation != 7) return run_action(argv[0], argv[1], NULL);
    g_autofree char *password = NULL;
    gsize length = 0;
    if (!g_file_get_contents("/dev/stdin", &password, &length, NULL) || !length)
        return backend_error(2, "missing-password", "Password Wi-Fi mancante");
    g_strchomp(password);
    int status = *password ? run_action("connect-secure", argv[1], password)
                          : backend_error(2, "missing-password", "Password Wi-Fi mancante");
    volatile unsigned char *wipe = (volatile unsigned char *)password;
    for (gsize i = 0; i < length; i++) wipe[i] = 0;
    return status;
}
