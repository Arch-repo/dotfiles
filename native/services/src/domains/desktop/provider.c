#include "service.h"
#include <stdio.h>
#include <unistd.h>

static int hypr_query(const char *operation, const char *header) {
    const char *args[] = {operation, "-j", NULL};
    BackendCommand result = backend_run_program("ANTO_MENU_HYPRCTL", "hyprctl", args);
    if (result.status != 0) {
        int status = backend_error(1, "compositor-unavailable", "Stato del compositor non disponibile");
        backend_command_clear(&result); return status;
    }
    json_object *json = json_tokener_parse(result.stdout_text);
    if (!json) { backend_command_clear(&result); return backend_error(1, "invalid-snapshot", "Dati del compositor non validi"); }
    if (header) g_print("%s\n", header);
    g_print("%s\n", json_object_to_json_string_ext(json, JSON_C_TO_STRING_PLAIN));
    json_object_put(json); backend_command_clear(&result); return 0;
}
static int keyboard(int argc, char **argv) {
    (void)argc; (void)argv;
    const char *args[] = {"-x", "fcitx5", NULL};
    BackendCommand process = backend_run_program("ANTO_MENU_PGREP", "pgrep", args);
    g_print("FCITX\t%s\n", process.status == 0 ? "active" : "inactive");
    backend_command_clear(&process);
    return hypr_query("devices", "DEVICES");
}
static int floating(int argc, char **argv) { (void)argc; (void)argv; return hypr_query("activewindow", NULL); }
static int background(int argc, char **argv) {
    (void)argc; (void)argv;
    g_autofree char *user = g_strdup_printf("%u", (unsigned)getuid());
    const char *args[] = {"-u", user, "-o", "%cpu=,rss=", NULL};
    BackendCommand processes = backend_run_program("ANTO_MENU_PS", "ps", args);
    double cpu = 0, memory = 0;
    g_auto(GStrv) lines = g_strsplit(processes.stdout_text ? processes.stdout_text : "", "\n", -1);
    for (guint i = 0; lines[i]; i++) {
        char *end = NULL;
        double load = g_ascii_strtod(lines[i], &end);
        if (!end || end == lines[i]) continue;
        double rss = g_ascii_strtod(end, NULL);
        cpu += load; memory += rss;
    }
    backend_command_clear(&processes);
    g_print("CPU\t%.1f%% CPU\nMEMORY\t%.1f GiB RAM\n", cpu, memory / 1048576.0);
    return hypr_query("clients", "CLIENTS");
}
static int notifications(int argc, char **argv) {
    (void)argc; (void)argv;
    const char *dnd_args[] = {"-D", "-sw", NULL};
    const char *count_args[] = {"-c", "-sw", NULL};
    BackendCommand dnd = backend_run_program("ANTO_MENU_SWAYNC_CLIENT", "swaync-client", dnd_args);
    BackendCommand count = backend_run_program("ANTO_MENU_SWAYNC_CLIENT", "swaync-client", count_args);
    g_autofree char *state = backend_first_line(dnd.stdout_text);
    g_autofree char *total = backend_first_line(count.stdout_text);
    gboolean valid = dnd.status == 0 && (g_strcmp0(state, "true") == 0 || g_strcmp0(state, "false") == 0);
    char *end = NULL;
    guint64 number = g_ascii_strtoull(total, &end, 10);
    g_print("%s\t%u\n", valid ? state : "unknown", count.status == 0 && end != total && !*end ? (guint)MIN(number, G_MAXUINT) : 0);
    backend_command_clear(&dnd); backend_command_clear(&count); return 0;
}
static const BackendOperation operations[] = {{"snapshot", 0, 0, FALSE}};
const BackendService anto_service_keyboard = {"keyboard", operations, G_N_ELEMENTS(operations), keyboard};
const BackendService anto_service_floating = {"floating", operations, G_N_ELEMENTS(operations), floating};
const BackendService anto_service_background = {"background", operations, G_N_ELEMENTS(operations), background};
const BackendService anto_service_notifications = {"notifications", operations, G_N_ELEMENTS(operations), notifications};
