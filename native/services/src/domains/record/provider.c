#include "backend.h"
#include "service.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static char *runtime_path(const char *name) {
    const char *runtime = g_get_user_runtime_dir();
    if (!runtime || !*runtime) runtime = "/tmp";
    return g_build_filename(runtime, name, NULL);
}

static char *recording_directory(void) {
    const char *override = g_getenv("ANTO_MENU_RECORDING_DIR");
    if (override && *override) return g_strdup(override);
    const char *videos = g_get_user_special_dir(G_USER_DIRECTORY_VIDEOS);
    g_autofree char *fallback = NULL;
    if (!videos || !*videos) {
        fallback = g_build_filename(g_get_home_dir(), "Videos", NULL);
        videos = fallback;
    }
    return g_build_filename(videos, "Recordings", NULL);
}

static gboolean parse_pid(const char *text, GPid *pid) {
    char *end = NULL;
    gint64 parsed = g_ascii_strtoll(text ? text : "", &end, 10);
    while (end && g_ascii_isspace(*end)) end++;
    if (!end || end == text || *end || parsed <= 1 || parsed > G_MAXINT)
        return FALSE;
    *pid = (GPid)parsed;
    return TRUE;
}

static gboolean recording_pid(GPid *pid) {
    g_autofree char *path =
        runtime_path("anto426-native-record.pid");
    g_autofree char *text = NULL;
    if (!g_file_get_contents(path, &text, NULL, NULL) ||
        !parse_pid(text, pid))
        return FALSE;
    return kill(*pid, 0) == 0 || errno == EPERM;
}

static char *capture_line(const char *environment, const char *fallback,
                          const char *const arguments[]) {
    const char *program = backend_program(environment, fallback);
    guint count = 0;
    while (arguments && arguments[count]) count++;
    const char **argv = g_new0(const char *, count + 2);
    argv[0] = program;
    for (guint index = 0; index < count; index++)
        argv[index + 1] = arguments[index];
    BackendCommand result = backend_command_run(argv, NULL);
    g_free(argv);
    char *line = result.status == 0
                     ? g_strdup(result.stdout_text)
                     : g_strdup("");
    g_strstrip(line);
    backend_command_clear(&result);
    return line;
}

static char *active_window_geometry(void) {
    const char *args[] = {"activewindow", "-j", NULL};
    g_autofree char *json =
        capture_line("ANTO_MENU_HYPRCTL", "hyprctl", args);
    struct json_object *root = json_tokener_parse(json);
    if (!root) return g_strdup("");
    struct json_object *at = NULL;
    struct json_object *size = NULL;
    char *geometry = g_strdup("");
    if (json_object_object_get_ex(root, "at", &at) &&
        json_object_object_get_ex(root, "size", &size) &&
        json_object_is_type(at, json_type_array) &&
        json_object_is_type(size, json_type_array) &&
        json_object_array_length(at) >= 2 &&
        json_object_array_length(size) >= 2) {
        int x = json_object_get_int(json_object_array_get_idx(at, 0));
        int y = json_object_get_int(json_object_array_get_idx(at, 1));
        int width =
            json_object_get_int(json_object_array_get_idx(size, 0));
        int height =
            json_object_get_int(json_object_array_get_idx(size, 1));
        g_free(geometry);
        geometry = g_strdup_printf("%d,%d %dx%d", x, y, width, height);
    }
    json_object_put(root);
    return geometry;
}

static char *focused_monitor(void) {
    const char *args[] = {"monitors", "-j", NULL};
    g_autofree char *json =
        capture_line("ANTO_MENU_HYPRCTL", "hyprctl", args);
    struct json_object *root = json_tokener_parse(json);
    if (!root || !json_object_is_type(root, json_type_array)) {
        if (root) json_object_put(root);
        return g_strdup("");
    }
    char *name = g_strdup("");
    for (guint index = 0; index < json_object_array_length(root); index++) {
        struct json_object *entry =
            json_object_array_get_idx(root, index);
        struct json_object *focused = NULL;
        struct json_object *value = NULL;
        if (json_object_object_get_ex(entry, "focused", &focused) &&
            json_object_get_boolean(focused) &&
            json_object_object_get_ex(entry, "name", &value)) {
            g_free(name);
            name = g_strdup(json_object_get_string(value));
            break;
        }
    }
    json_object_put(root);
    return name;
}

static int start_recording(const char *mode, gboolean audio) {
    if (g_strcmp0(mode, "area") != 0 &&
        g_strcmp0(mode, "window") != 0 &&
        g_strcmp0(mode, "monitor") != 0)
        return backend_error(2, "invalid-record-mode",
                             "Modalità di registrazione non valida");
    GPid current = 0;
    if (recording_pid(&current))
        return backend_error(1, "already-recording",
                             "Una registrazione è già attiva");
    if (backend_dry_run()) {
        g_print("DRYRUN\trecord\t%s\t%s\n", mode,
                audio ? "audio" : "video");
        return 0;
    }
    g_autofree char *geometry = NULL;
    g_autofree char *output = NULL;
    if (g_strcmp0(mode, "area") == 0) {
        const char *args[] = {NULL};
        geometry = capture_line("ANTO_MENU_SLURP", "slurp", args);
        if (!*geometry) return 0;
    } else if (g_strcmp0(mode, "window") == 0) {
        geometry = active_window_geometry();
        if (!*geometry)
            return backend_error(1, "window",
                                 "Geometria della finestra non disponibile");
    } else if (g_strcmp0(mode, "monitor") == 0) {
        output = focused_monitor();
        if (!*output)
            return backend_error(1, "monitor",
                                 "Monitor attivo non disponibile");
    }

    g_autofree char *directory = recording_directory();
    if (g_mkdir_with_parents(directory, 0700) != 0)
        return backend_error(1, "mkdir",
                             "Impossibile creare la cartella Registrazioni");
    g_autoptr(GDateTime) now = g_date_time_new_now_local();
    g_autofree char *stamp =
        g_date_time_format(now, "%Y-%m-%d_%H-%M-%S");
    g_autofree char *filename =
        g_strdup_printf("record-%s.mp4", stamp);
    g_autofree char *file =
        g_build_filename(directory, filename, NULL);
    g_autofree char *log =
        runtime_path("anto426-native-record.log");
    const char *program =
        backend_program("ANTO_MENU_WF_RECORDER", "wf-recorder");
    const char *argv[12] = {program, "-f", file};
    guint index = 3;
    if (geometry && *geometry) {
        argv[index++] = "-g";
        argv[index++] = geometry;
    }
    if (output && *output) {
        argv[index++] = "-o";
        argv[index++] = output;
    }
    if (audio) argv[index++] = "--audio";
    argv[index] = NULL;

    g_autoptr(GSubprocessLauncher) launcher =
        g_subprocess_launcher_new(G_SUBPROCESS_FLAGS_NONE);
    g_subprocess_launcher_set_stdout_file_path(launcher, log);
    g_subprocess_launcher_set_stderr_file_path(launcher, log);
    g_autoptr(GError) error = NULL;
    g_autoptr(GSubprocess) process =
        g_subprocess_launcher_spawnv(launcher, argv, &error);
    if (!process)
        return backend_error(1, "recorder",
                             error ? error->message :
                                     "Impossibile avviare wf-recorder");
    const char *identifier = g_subprocess_get_identifier(process);
    GPid pid = 0;
    if (!parse_pid(identifier, &pid))
        return backend_error(1, "pid",
                             "PID della registrazione non disponibile");
    g_autofree char *pid_path =
        runtime_path("anto426-native-record.pid");
    g_autofree char *pid_text = g_strdup_printf("%d\n", pid);
    if (!g_file_set_contents(pid_path, pid_text, -1, &error))
        return backend_error(1, "pid-file",
                             error ? error->message :
                                     "Impossibile salvare il PID");
    backend_notify("Registrazione avviata", filename);
    return 0;
}

static int stop_recording(void) {
    GPid pid = 0;
    if (!recording_pid(&pid)) return 0;
    if (backend_dry_run()) {
        g_print("DRYRUN\trecord\tstop\t%d\n", pid);
        return 0;
    }
    if (kill(pid, SIGINT) != 0 && errno != ESRCH)
        return backend_error(1, "stop",
                             "Impossibile fermare la registrazione");
    g_autofree char *path =
        runtime_path("anto426-native-record.pid");
    g_unlink(path);
    backend_notify("Registrazione", "Video salvato");
    return 0;
}

static int record_status(gboolean waybar) {
    GPid pid = 0;
    gboolean active = recording_pid(&pid);
    if (waybar) {
        if (active)
            g_print("{\"text\":\"󰑊\",\"class\":\"recording\","
                    "\"tooltip\":\"Registrazione attiva · PID %d\"}\n",
                    pid);
        else
            puts("{\"text\":\"󰄀\",\"class\":\"idle\","
                 "\"tooltip\":\"Screenshot e registrazione\"}");
    } else if (active) {
        g_print("REC · PID %d\n", pid);
    } else {
        puts("Pronto");
    }
    return 0;
}

static int open_recordings(void) {
    g_autofree char *directory = recording_directory();
    if (!backend_dry_run() &&
        g_mkdir_with_parents(directory, 0700) != 0)
        return backend_error(1, "mkdir",
                             "Impossibile creare la cartella Registrazioni");
    if (backend_dry_run()) {
        g_print("DRYRUN\trecord\topen\t%s\n", directory);
        return 0;
    }
    const char *program =
        backend_program("ANTO_MENU_XDG_OPEN", "xdg-open");
    const char *argv[] = {program, directory, NULL};
    return backend_command_forward(argv, NULL);
}

int anto_record_execute(int argc, char **argv) {
    (void)argc;

    extern const BackendService anto_service_record;
    int operation = backend_operation_index(&anto_service_record, argv[0]);
    static const char *targets[] = {"area", "window", "monitor", "monitor"};
    if (operation >= 0 && operation < 4) return start_recording(targets[operation], operation == 3);
    switch (operation) {
        case 4: return stop_recording();
        case 5: return record_status(FALSE);
        case 6: return record_status(TRUE);
        case 7: return open_recordings();
        default: return 2;
    }
}
