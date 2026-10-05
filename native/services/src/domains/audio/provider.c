#include "backend.h"
#include "service.h"

#include <stdio.h>
#include <math.h>
#include <string.h>

static char *wpctl_value(const char *first, const char *second) {
    const char *args[] = {first, second, NULL};
    BackendCommand result =
        backend_run_program("ANTO_MENU_WPCTL", "wpctl", args);
    char *value =
        result.status == 0 ? backend_first_line(result.stdout_text) : g_strdup("");
    backend_command_clear(&result);
    return value;
}

static char *sink_name(void) {
    const char *args[] = {
        "inspect", "@DEFAULT_AUDIO_SINK@", NULL,
    };
    BackendCommand result =
        backend_run_program("ANTO_MENU_WPCTL", "wpctl", args);
    char *name = g_strdup("");
    g_auto(GStrv) lines = g_strsplit(result.stdout_text, "\n", -1);
    for (guint index = 0; lines[index]; index++) {
        char *description = strstr(lines[index], "node.description = ");
        if (!description) continue;
        description += strlen("node.description = ");
        g_strstrip(description);
        g_free(name);
        name = g_strdup(description);
        g_strdelimit(name, "\"", ' ');
        g_strstrip(name);
        break;
    }
    backend_command_clear(&result);
    return name;
}

static char *playerctl_value(const char *first, const char *second,
                             const char *third) {
    const char *args[] = {first, second, third, NULL};
    BackendCommand result =
        backend_run_program("ANTO_MENU_PLAYERCTL", "playerctl", args);
    char *value =
        result.status == 0 ? backend_first_line(result.stdout_text) : g_strdup("");
    backend_command_clear(&result);
    return value;
}

static void print_clean(const char *value) {
    g_autofree char *clean = backend_clean_field(value);
    fputs(clean, stdout);
}

static int audio_snapshot(void) {
    g_autofree char *sink =
        wpctl_value("get-volume", "@DEFAULT_AUDIO_SINK@");
    g_autofree char *source =
        wpctl_value("get-volume", "@DEFAULT_AUDIO_SOURCE@");
    g_autofree char *name = sink_name();
    g_autofree char *player =
        playerctl_value("metadata", "--format", "{{playerName}}");
    g_autofree char *state =
        playerctl_value("status", NULL, NULL);
    g_autofree char *artist =
        playerctl_value("metadata", "artist", NULL);
    g_autofree char *title =
        playerctl_value("metadata", "title", NULL);
    g_autofree char *album =
        playerctl_value("metadata", "album", NULL);
    fputs("SINK\t", stdout);
    print_clean(sink);
    fputs("\nSOURCE\t", stdout);
    print_clean(source);
    fputs("\nNAME\t", stdout);
    print_clean(name);
    fputs("\nMPRIS\t", stdout);
    print_clean(player);
    fputc('\t', stdout);
    print_clean(state);
    fputc('\t', stdout);
    print_clean(artist);
    fputc('\t', stdout);
    print_clean(title);
    fputc('\t', stdout);
    print_clean(album);
    fputc('\n', stdout);
    return 0;
}

static gboolean valid_target(const char *target) {
    return g_strcmp0(target, "@DEFAULT_AUDIO_SINK@") == 0 ||
           g_strcmp0(target, "@DEFAULT_AUDIO_SOURCE@") == 0;
}

static int audio_wpctl_action(const char *operation, const char *target,
                              const char *value) {
    if (!valid_target(target))
        return backend_error(2, "invalid-target",
                             "Destinazione audio non valida");
    if (backend_dry_run()) {
        g_print("DRYRUN\taudio\t%s\t%s\t%s\n", operation, target,
                value ? value : "-");
        return 0;
    }
    const char *args[] = {operation, target, value, NULL};
    BackendCommand result =
        backend_run_program("ANTO_MENU_WPCTL", "wpctl", args);
    if (result.status != 0 && result.stderr_text)
        fputs(result.stderr_text, stderr);
    int status = result.status;
    backend_command_clear(&result);
    return status;
}

static int audio_player_action(const char *operation) {
    if (g_strcmp0(operation, "previous") != 0 &&
        g_strcmp0(operation, "play-pause") != 0 &&
        g_strcmp0(operation, "next") != 0)
        return backend_error(2, "invalid-player-action",
                             "Azione player non valida");
    if (backend_dry_run()) {
        g_print("DRYRUN\taudio\tplayer\t%s\n", operation);
        return 0;
    }
    const char *args[] = {operation, NULL};
    BackendCommand result =
        backend_run_program("ANTO_MENU_PLAYERCTL", "playerctl", args);
    if (result.status != 0 && result.stderr_text)
        fputs(result.stderr_text, stderr);
    int status = result.status;
    backend_command_clear(&result);
    return status;
}

int anto_audio_execute(int argc, char **argv) {
    (void)argc;

    extern const BackendService anto_service_audio;
    int operation = backend_operation_index(&anto_service_audio, argv[0]);
    if (operation == 0) return audio_snapshot();
    if (operation == 4) {
        if (backend_dry_run()) { g_print("DRYRUN\taudio\tmixer\t-\n"); return 0; }
        const char *command[] = {backend_program("ANTO_MENU_PAVUCONTROL", "pavucontrol"), NULL};
        return backend_command_forward(command, NULL);
    }
    if (operation == 3) return audio_player_action(argv[1]);
    if (!valid_target(argv[1])) return backend_error(2, "invalid-target", "Destinazione audio non valida");
    if (operation == 1) {
        char *end = NULL;
        double value = g_ascii_strtod(argv[2], &end);
        if (!end || end == argv[2] || *end || !isfinite(value) || value < 0 || value > 1.5)
            return backend_error(2, "invalid-volume", "Volume richiesto da 0 a 1.5");
        return audio_wpctl_action("set-volume", argv[1], argv[2]);
    }
    return audio_wpctl_action("set-mute", argv[1], "toggle");
}
