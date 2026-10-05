#include "backend.h"
#include "service.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static BackendCommand run_brightnessctl(const char *const arguments[]) {
    return backend_run_program("ANTO_MENU_BRIGHTNESSCTL", "brightnessctl", arguments);
}

static BackendCommand run_profiles(const char *const arguments[]) {
    return backend_run_program("ANTO_MENU_POWERPROFILESCTL", "powerprofilesctl", arguments);
}

static char *read_file_trimmed(const char *path) {
    g_autofree char *content = NULL;
    if (!g_file_get_contents(path, &content, NULL, NULL))
        return g_strdup("");
    g_strstrip(content);
    return g_steal_pointer(&content);
}

static void battery_state(char **capacity, char **status) {
    const char *root =
        backend_program("ANTO_MENU_POWER_SUPPLY_ROOT",
                        "/sys/class/power_supply");
    g_autoptr(GDir) directory = g_dir_open(root, 0, NULL);
    if (!directory) return;
    const char *name = NULL;
    while ((name = g_dir_read_name(directory))) {
        if (!g_str_has_prefix(name, "BAT")) continue;
        g_autofree char *capacity_path =
            g_build_filename(root, name, "capacity", NULL);
        if (!g_file_test(capacity_path, G_FILE_TEST_IS_REGULAR)) continue;
        g_autofree char *status_path =
            g_build_filename(root, name, "status", NULL);
        g_free(*capacity);
        g_free(*status);
        *capacity = read_file_trimmed(capacity_path);
        *status = read_file_trimmed(status_path);
        return;
    }
}

static int energy_snapshot(void) {
    g_autofree char *brightness = g_strdup("");
    {
        const char *args[] = {"-m", NULL};
        BackendCommand result = run_brightnessctl(args);
        if (result.status == 0) {
            g_autofree char *line = backend_first_line(result.stdout_text);
            g_auto(GStrv) fields = g_strsplit(line, ",", -1);
            if (g_strv_length(fields) >= 4) {
                g_strstrip(fields[3]);
                g_free(brightness);
                brightness = g_strdup(fields[3]);
                g_strdelimit(brightness, "%", '\0');
            }
        }
        backend_command_clear(&result);
    }
    g_autofree char *capacity = g_strdup("");
    g_autofree char *status = g_strdup("");
    battery_state(&capacity, &status);
    g_autofree char *profile = NULL;
    {
        const char *args[] = {"get", NULL};
        BackendCommand result = run_profiles(args);
        profile = result.status == 0 ? backend_first_line(result.stdout_text)
                                     : g_strdup("non disponibile");
        backend_command_clear(&result);
    }
    g_print("brightness\t%s\ncapacity\t%s\nstatus\t%s\nprofile\t%s\n",
            brightness, capacity, status,
            *profile ? profile : "non disponibile");
    return 0;
}

static gboolean valid_profile(const char *profile) {
    return g_strcmp0(profile, "power-saver") == 0 ||
           g_strcmp0(profile, "balanced") == 0 ||
           g_strcmp0(profile, "performance") == 0;
}

static int profile_action(const char *operation, const char *profile) {
    if (g_strcmp0(operation, "set") == 0 && !valid_profile(profile))
        return backend_error(2, "invalid-profile",
                             "Profilo energetico non valido");
    if (backend_dry_run()) {
        g_print("DRYRUN\tenergy\t%s\t%s\n", operation,
                profile ? profile : "-");
        return 0;
    }
    const char *args[] = {operation, profile, NULL};
    BackendCommand result = run_profiles(args);
    if (result.stdout_text) fputs(result.stdout_text, stdout);
    if (result.status != 0 && result.stderr_text)
        fputs(result.stderr_text, stderr);
    int status = result.status;
    backend_command_clear(&result);
    if (status == 0 && g_strcmp0(operation, "set") == 0)
        backend_notify("Profilo energetico", "Profilo applicato");
    return status;
}

static int brightness_set(const char *text) {
    char *end = NULL;
    double value = g_ascii_strtod(text ? text : "", &end);
    if (!end || end == text || *end || !isfinite(value) ||
        value < 1.0 || value > 100.0)
        return backend_error(2, "invalid-brightness",
                             "Luminosità richiesta da 1 a 100");
    int rounded = (int)llround(value);
    g_autofree char *percent = g_strdup_printf("%d%%", rounded);
    if (backend_dry_run()) {
        g_print("DRYRUN\tenergy\tbrightness-set\t%s\n", percent);
        return 0;
    }
    const char *args[] = {"set", percent, NULL};
    BackendCommand result = run_brightnessctl(args);
    if (result.status != 0 && result.stderr_text)
        fputs(result.stderr_text, stderr);
    int status = result.status;
    backend_command_clear(&result);
    return status;
}

int anto_energy_execute(int argc, char **argv) {
    (void)argc;

    extern const BackendService anto_service_energy;
    switch (backend_operation_index(&anto_service_energy, argv[0])) {
        case 0: return energy_snapshot();
        case 1: return profile_action("get", NULL);
        case 2: return profile_action("list", NULL);
        case 3: return profile_action("set", argv[1]);
        case 4: return brightness_set(argv[1]);
        default: return backend_error(2, "invalid-operation", "Operazione energetica non valida");
    }
}
