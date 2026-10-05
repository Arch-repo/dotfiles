#include "internal.h"
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

static int display_dispatch(DisplayContext *context, int argc, char **argv, json_object *monitors) {
    const char *name = argv[0];
    struct { const char *name, *label, *fallback; DisplayMutation mutation; } guarded[] = {
        {"enable", "Attivazione", NULL, anto_display_mutation_enable}, {"disable", "Disattivazione", NULL, anto_display_mutation_disable},
        {"toggle", "Cambio stato", NULL, anto_display_mutation_toggle}, {"only", "Solo", NULL, anto_display_mutation_only},
        {"extend", "Desktop esteso", "right", anto_display_mutation_extend}, {"mirror", "Duplicazione", NULL, anto_display_mutation_mirror},
        {"duplicate", "Duplicazione", NULL, anto_display_mutation_mirror}, {"scale", "Scala", NULL, anto_display_mutation_scale},
        {"transform", "Orientamento", NULL, anto_display_mutation_transform}, {"mode", "Modalità", NULL, anto_display_mutation_mode},
        {"position", "Posizione", NULL, anto_display_mutation_position}, {"profile-apply", "Profilo", "last", anto_display_mutation_profile},
    };
    for (guint i = 0; i < G_N_ELEMENTS(guarded); i++)
        if (g_strcmp0(name, guarded[i].name) == 0)
            return anto_display_guarded_named(context, guarded[i].label, guarded[i].mutation,
                                  argc > 1 ? argv[1] : guarded[i].fallback, argc > 2 ? argv[2] : NULL);
    if (g_strcmp0(name, "only-primary") == 0 || g_strcmp0(name, "only-external") == 0) {
        const char *target = anto_display_primary_output(monitors);
        if (g_strcmp0(name, "only-external") == 0) {
            const char *primary = target; target = "";
            for (size_t i = 0; i < json_object_array_length(monitors); i++) {
                const char *candidate = anto_display_member_string(json_object_array_get_idx(monitors, i), "name", "");
                if (g_strcmp0(candidate, primary) != 0) { target = candidate; break; }
            }
        }
        return *target ? anto_display_guarded_named(context, "Solo", anto_display_mutation_only, target, NULL)
                       : anto_display_display_error("no-output", "Monitor richiesto non disponibile");
    }
    extern const BackendService anto_service_display;
    switch (backend_operation_index(&anto_service_display, name)) {
        case 0: return anto_display_action_status(monitors);
        case 1: return anto_display_action_list(monitors);
        case 2: return anto_display_action_modes(monitors, argv[1]);
        case 16: return anto_display_action_arrange(context, argv[1]);
        case 17: return anto_display_action_focus(context, monitors, argv[1]);
        case 18: case 19: return anto_display_action_dpms(context, monitors, g_strcmp0(name, "dpms-on") == 0 ? "on" : "off", argc > 1 ? argv[1] : NULL);
        case 20: return anto_display_action_persistent_preview(context, monitors);
        case 21: return anto_display_action_persist_current(context, monitors);
        case 22: return anto_display_action_profile_save(context, monitors, argc > 1 ? argv[1] : "current");
        case 24: return anto_display_action_profile_list(context);
        case 25: return anto_display_action_profile_delete(context, argv[1]);
        case 26: return anto_display_action_editor();
        case 27: case 28: case 29: return anto_display_action_help();
        default: return backend_error(2, "invalid-operation", "Operazione schermi non valida");
    }
}

int anto_display_execute(int argc, char **argv) {
    if (argc < 1) return backend_usage("display", "AZIONE [ARGOMENTI]");
    DisplayContext context = anto_display_display_context();
    json_object *monitors = NULL;
    gboolean needs_monitors =
        !(g_strcmp0(argv[0], "profile-list") == 0 ||
          g_strcmp0(argv[0], "profile-delete") == 0 ||
          g_strcmp0(argv[0], "editor") == 0 ||
          g_strcmp0(argv[0], "help") == 0 ||
          g_strcmp0(argv[0], "-h") == 0 ||
          g_strcmp0(argv[0], "--help") == 0);
    g_autoptr(GError) error = NULL;
    if (needs_monitors) {
        monitors = anto_display_monitor_json(&error);
        if (!monitors) {
            int result = anto_display_display_error("state", anto_display_display_error_message(error));
            anto_display_display_context_clear(&context);
            return result;
        }
    } else {
        monitors = json_object_new_array();
    }
    int result = display_dispatch(&context, argc, argv, monitors);
    json_object_put(monitors);
    anto_display_display_context_clear(&context);
    return result;
}
