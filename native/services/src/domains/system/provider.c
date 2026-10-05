#include "backend.h"
#include "service.h"

#include <stdio.h>
#include <string.h>
#include <sys/statvfs.h>
#include <sys/utsname.h>

static char *command_line(const char *environment, const char *fallback,
                          const char *const args[]) {
    BackendCommand result = backend_run_program(environment, fallback, args);
    char *line =
        result.status == 0 ? backend_first_line(result.stdout_text) : g_strdup("");
    backend_command_clear(&result);
    return line;
}

static char *system_volume(void) {
    const char *args[] = {
        "get-volume", "@DEFAULT_AUDIO_SINK@", NULL,
    };
    g_autofree char *line =
        command_line("ANTO_MENU_WPCTL", "wpctl", args);
    const char *colon = strchr(line, ':');
    if (!colon) return g_strdup("n/d");
    char *end = NULL;
    double volume = g_ascii_strtod(colon + 1, &end);
    if (!end || end == colon + 1) return g_strdup("n/d");
    return g_strdup_printf("%d%%%s", (int)(volume * 100.0 + 0.5),
                           strstr(line, "MUTED") ? " MUTE" : "");
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
    g_ptr_array_add(fields, g_string_free(field, FALSE));
    return fields;
}

static const char *at(GPtrArray *fields, guint index) {
    return fields && index < fields->len
               ? g_ptr_array_index(fields, index)
               : "";
}

static char *system_network(void) {
    const char *args[] = {
        "-t", "--escape", "yes", "-f",
        "TYPE,STATE,CONNECTION", "device", "status", NULL,
    };
    BackendCommand result =
        backend_run_program("ANTO_MENU_NMCLI", "nmcli", args);
    char *network = g_strdup("Disconnessa");
    g_auto(GStrv) lines = g_strsplit(result.stdout_text ? result.stdout_text : "", "\n", -1);
    for (guint index = 0; lines && lines[index]; index++) {
        g_autoptr(GPtrArray) fields = split_terse(lines[index]);
        if ((g_strcmp0(at(fields, 0), "wifi") == 0 || g_strcmp0(at(fields, 0), "ethernet") == 0) &&
            g_strcmp0(at(fields, 1), "connected") == 0) {
            g_free(network);
            network = g_strdup(*at(fields, 2)
                                   ? at(fields, 2) : "Connessa");
            break;
        }
    }
    backend_command_clear(&result);
    return network;
}

static char *system_brightness(void) {
    const char *args[] = {"-m", NULL};
    g_autofree char *line = command_line(
        "ANTO_MENU_BRIGHTNESSCTL", "brightnessctl", args);
    g_auto(GStrv) fields = g_strsplit(line, ",", -1);
    if (g_strv_length(fields) <= 3 || !*fields[3]) return g_strdup("n/d");
    g_strstrip(fields[3]);
    return g_strdup(fields[3]);
}

static char *system_displays(void) {
    const char *args[] = {"-j", "monitors", NULL};
    BackendCommand result =
        backend_run_program("ANTO_MENU_HYPRCTL", "hyprctl", args);
    int count = -1;
    if (result.status == 0) {
        struct json_object *root =
            json_tokener_parse(result.stdout_text);
        if (root && json_object_is_type(root, json_type_array))
            count = (int)json_object_array_length(root);
        if (root) json_object_put(root);
    }
    backend_command_clear(&result);
    if (count < 0) return g_strdup("n/d");
    return g_strdup_printf("%d attiv%s", count, count == 1 ? "o" : "i");
}

static gboolean bluetooth_powered(const char *text) {
    g_auto(GStrv) lines = g_strsplit(text ? text : "", "\n", -1);
    for (guint index = 0; lines[index]; index++) {
        g_strstrip(lines[index]);
        if (g_ascii_strncasecmp(lines[index], "Powered:", 8) == 0)
            return strstr(lines[index] + 8, "yes") != NULL;
    }
    return FALSE;
}

static char *system_bluetooth(void) {
    const char *show_args[] = {"show", NULL};
    BackendCommand show = backend_run_program(
        "ANTO_MENU_BLUETOOTHCTL", "bluetoothctl", show_args);
    if (show.status != 0 || !bluetooth_powered(show.stdout_text)) {
        backend_command_clear(&show);
        return g_strdup("Spento");
    }
    backend_command_clear(&show);
    const char *devices_args[] = {"devices", "Connected", NULL};
    BackendCommand devices = backend_run_program(
        "ANTO_MENU_BLUETOOTHCTL", "bluetoothctl", devices_args);
    guint count = 0;
    if (devices.status == 0 && devices.stdout_text) {
        g_auto(GStrv) lines = g_strsplit(devices.stdout_text, "\n", -1);
        for (guint index = 0; lines && lines[index]; index++)
            if (g_str_has_prefix(lines[index], "Device ")) count++;
    }
    backend_command_clear(&devices);
    if (!count) return g_strdup("Attivo");
    return g_strdup_printf("%u conness%s", count, count == 1 ? "o" : "i");
}

static void print_key(const char *key, const char *value) {
    g_autofree char *clean = backend_clean_field(value);
    g_print("%s\t%s\n", key, clean);
}

static int desktop_snapshot(void) {
    g_autofree char *volume = system_volume();
    g_autofree char *network = system_network();
    g_autofree char *brightness = system_brightness();
    g_autofree char *displays = system_displays();
    g_autofree char *bluetooth = system_bluetooth();
    print_key("volume", volume);
    print_key("network", network);
    print_key("brightness", brightness);
    print_key("displays", displays);
    print_key("bluetooth", bluetooth);
    const char *power = backend_program("ANTO_MENU_POWER_SUPPLY_ROOT", "/sys/class/power_supply");
    g_autoptr(GDir) directory = g_dir_open(power, 0, NULL);
    const char *name = NULL;
    gboolean battery_seen = FALSE;
    while (directory && (name = g_dir_read_name(directory))) {
        if (!g_str_has_prefix(name, "BAT")) continue;
        g_autofree char *path = g_build_filename(power, name, "capacity", NULL);
        g_autofree char *capacity = NULL;
        if (!g_file_get_contents(path, &capacity, NULL, NULL)) continue;
        g_strstrip(capacity);
        g_autofree char *text = g_strdup_printf("%s%%", capacity);
        print_key("battery", text);
        battery_seen = TRUE;
        break;
    }
    if (!battery_seen) print_key("battery", "Non rilevata");
    return 0;
}

static char *read_trimmed(const char *path) {
    g_autofree char *value = NULL;
    if (!g_file_get_contents(path, &value, NULL, NULL))
        return g_strdup("");
    g_strstrip(value);
    return g_steal_pointer(&value);
}

static char *hardware_load(void) {
    const char *proc_root =
        backend_program("ANTO_MENU_PROC_ROOT", "/proc");
    g_autofree char *path = g_build_filename(proc_root, "loadavg", NULL);
    g_autofree char *value = read_trimmed(path);
    g_auto(GStrv) fields = g_strsplit(value, " ", 4);
    return g_strdup(g_strv_length(fields) > 2 ? fields[2] : "");
}

static guint64 meminfo_value(const char *content, const char *key) {
    g_auto(GStrv) lines = g_strsplit(content ? content : "", "\n", -1);
    for (guint index = 0; lines[index]; index++) {
        if (!g_str_has_prefix(lines[index], key)) continue;
        const char *cursor = lines[index] + strlen(key);
        while (*cursor && !g_ascii_isdigit(*cursor)) cursor++;
        return g_ascii_strtoull(cursor, NULL, 10) * 1024;
    }
    return 0;
}

static char *format_size(guint64 bytes) {
    static const char *const units[] = {"B", "KiB", "MiB", "GiB", "TiB"};
    double value = (double)bytes;
    guint unit = 0;
    while (value >= 1024.0 && unit + 1 < G_N_ELEMENTS(units)) {
        value /= 1024.0;
        unit++;
    }
    return g_strdup_printf(value >= 10.0 ? "%.0f %s" : "%.1f %s",
                           value, units[unit]);
}

static char *hardware_memory(void) {
    const char *proc_root =
        backend_program("ANTO_MENU_PROC_ROOT", "/proc");
    g_autofree char *path = g_build_filename(proc_root, "meminfo", NULL);
    g_autofree char *content = read_trimmed(path);
    guint64 total = meminfo_value(content, "MemTotal:");
    guint64 available = meminfo_value(content, "MemAvailable:");
    guint64 used = total > available ? total - available : 0;
    g_autofree char *used_text = format_size(used);
    g_autofree char *total_text = format_size(total);
    return g_strdup_printf("%s / %s", used_text, total_text);
}

static char *hardware_disk(void) {
    const char *root =
        backend_program("ANTO_MENU_FILESYSTEM_ROOT", "/");
    struct statvfs info = {0};
    if (statvfs(root, &info) != 0) return g_strdup("");
    guint64 total = (guint64)info.f_blocks * info.f_frsize;
    guint64 free = (guint64)info.f_bavail * info.f_frsize;
    guint64 used = total > free ? total - free : 0;
    g_autofree char *used_text = format_size(used);
    g_autofree char *free_text = format_size(free);
    return g_strdup_printf("%s usati · %s liberi", used_text, free_text);
}

static char *hardware_temperature(void) {
    const char *root = backend_program(
        "ANTO_MENU_THERMAL_ROOT", "/sys/class/thermal");
    g_autoptr(GDir) directory = g_dir_open(root, 0, NULL);
    if (!directory) return g_strdup("");
    const char *name = NULL;
    while ((name = g_dir_read_name(directory))) {
        if (!g_str_has_prefix(name, "thermal_zone")) continue;
        g_autofree char *path = g_build_filename(root, name, "temp", NULL);
        g_autofree char *text = read_trimmed(path);
        char *end = NULL;
        double value = g_ascii_strtod(text, &end);
        if (end && end != text) {
            if (value > 1000.0) value /= 1000.0;
            return g_strdup_printf("%.0f°C", value);
        }
    }
    return g_strdup("");
}

static char *hardware_kernel(void) {
    struct utsname info = {0};
    return uname(&info) == 0 ? g_strdup(info.release) : g_strdup("");
}

static char *hardware_profile(void) {
    const char *args[] = {"get", NULL};
    g_autofree char *profile = command_line(
        "ANTO_MENU_POWERPROFILESCTL", "powerprofilesctl", args);
    return *profile ? g_steal_pointer(&profile)
                    : g_strdup("non disponibile");
}

static int hardware_snapshot(void) {
    g_autofree char *cpu = hardware_load();
    g_autofree char *memory = hardware_memory();
    g_autofree char *disk = hardware_disk();
    g_autofree char *temperature = hardware_temperature();
    g_autofree char *kernel = hardware_kernel();
    g_autofree char *profile = hardware_profile();
    print_key("CPU", cpu);
    print_key("MEMORY", memory);
    print_key("DISK", disk);
    print_key("TEMP", temperature);
    print_key("KERNEL", kernel);
    print_key("PROFILE", profile);
    return 0;
}

int anto_system_execute(int argc, char **argv) {
    (void)argc;

    extern const BackendService anto_service_system;
    return backend_operation_index(&anto_service_system, argv[0]) == 0 ? desktop_snapshot() : hardware_snapshot();
}
