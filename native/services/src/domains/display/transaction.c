#include "internal.h"
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

const char *anto_display_display_error_message(const GError *error) {
    return error && error->message ? error->message
                                   : "Operazione schermi non riuscita";
}

int anto_display_display_error(const char *code, const char *message) {
    backend_notify("Schermi", message);
    return backend_error(1, code, message);
}

static char *confirmation_response(const char *description) {
    const char *preset = g_getenv("ANTO_DISPLAY_CONFIRM_RESPONSE");
    if (g_strcmp0(preset, "keep") == 0 || g_strcmp0(preset, "revert") == 0)
        return g_strdup(preset);
    if (backend_no_notify()) return g_strdup("revert");
    const char *notify = backend_program("ANTO_MENU_NOTIFY_SEND", "notify-send");
    const char *timeout = backend_program("ANTO_MENU_TIMEOUT", "timeout");
    g_autofree char *body = g_strdup_printf(
        "%s · ripristino automatico tra 15 secondi", description);
    const char *argv[] = {
        timeout, "--foreground", "--kill-after=1", "17",
        notify, "--wait", "--app-name=Anto Display", "--icon=video-display-symbolic",
        "--urgency=critical", "--expire-time=15000",
        "--action=keep=Mantieni", "--action=revert=Ripristina",
        "Conferma configurazione schermi", body, NULL,
    };
    BackendCommand command = backend_command_run(argv, NULL);
    char *answer = command.status == 0
                       ? backend_clean_field(command.stdout_text)
                       : g_strdup("revert");
    backend_command_clear(&command);
    if (g_strcmp0(answer, "keep") != 0) {
        g_free(answer);
        answer = g_strdup("revert");
    }
    return answer;
}

static int guarded_mutation(DisplayContext *context, const char *description,
                            DisplayMutation mutation, const char *first,
                            const char *second) {
    g_autoptr(GError) error = NULL;
    if (!anto_display_acquire_lock(context, &error))
        return anto_display_display_error("busy", anto_display_display_error_message(error));
    json_object *before = anto_display_monitor_json(&error);
    if (!before) return anto_display_display_error("state", anto_display_display_error_message(error));
    json_object *rollback = anto_display_snapshot_copy(before);
    context->mutation_applied = FALSE;
    gboolean changed = mutation(context, before, first, second, &error);
    json_object_put(before);
    if (!changed) {
        if (context->mutation_applied) {
            g_autoptr(GError) restore_error = NULL;
            (void)anto_display_restore_snapshot(context, rollback, &restore_error);
        }
        json_object_put(rollback);
        return anto_display_display_error("mutation", anto_display_display_error_message(error));
    }
    if (context->dry_run) {
        json_object_put(rollback);
        return 0;
    }
    g_autofree char *answer = confirmation_response(description);
    if (g_strcmp0(answer, "keep") != 0) {
        g_autoptr(GError) restore_error = NULL;
        gboolean restored = anto_display_restore_snapshot(context, rollback, &restore_error);
        json_object_put(rollback);
        if (!restored)
            return anto_display_display_error("rollback", anto_display_display_error_message(restore_error));
        backend_notify("Schermi", "Configurazione precedente ripristinata");
        return 0;
    }
    json_object *confirmed = anto_display_monitor_json(&error);
    if (!confirmed || !anto_display_persist_snapshot(context, confirmed, &error)) {
        if (confirmed) json_object_put(confirmed);
        g_autoptr(GError) restore_error = NULL;
        (void)anto_display_restore_snapshot(context, rollback, &restore_error);
        json_object_put(rollback);
        return anto_display_display_error("persist", anto_display_display_error_message(error));
    }
    g_autoptr(GError) quick_error = NULL;
    if (!anto_display_save_quick_profiles(context, confirmed, FALSE, &quick_error))
        backend_notify("Schermi",
                       "Layout salvato; profilo di ripristino non aggiornato");
    json_object_put(confirmed);
    json_object_put(rollback);
    backend_notify("Schermi",
                   "Configurazione mantenuta e salvata per il prossimo avvio");
    return 0;
}

int anto_display_guarded_named(DisplayContext *context, const char *label,
                         DisplayMutation mutation, const char *first,
                         const char *second) {
    g_autofree char *description = g_strdup_printf("%s%s%s", label,
                                                    first && *first ? " " : "",
                                                    first ? first : "");
    return guarded_mutation(context, description, mutation, first, second);
}
