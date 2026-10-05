#include "backend.h"

#include <errno.h>
#include <signal.h>
#include <string.h>
#include <sys/wait.h>

const char *backend_program(const char *environment, const char *fallback) {
    const char *override = environment ? g_getenv(environment) : NULL;
    return override && *override ? override : fallback;
}

gboolean backend_dry_run(void) {
    return g_strcmp0(g_getenv("ANTO_MENU_DRY_RUN"), "1") == 0;
}

gboolean backend_no_notify(void) {
    return g_strcmp0(g_getenv("ANTO_MENU_NO_NOTIFY"), "1") == 0;
}

gboolean backend_valid_address(const char *address) {
    if (!address || strlen(address) != 17) return FALSE;
    for (guint index = 0; index < 17; index++) {
        if (index % 3 == 2) {
            if (address[index] != ':') return FALSE;
        } else if (!g_ascii_isxdigit(address[index])) {
            return FALSE;
        }
    }
    return TRUE;
}

gboolean backend_valid_token(const char *value) {
    if (!value || !*value) return FALSE;
    for (const char *cursor = value; *cursor; cursor++)
        if (!(g_ascii_isalnum(*cursor) || strchr("_.:+@%-", *cursor)))
            return FALSE;
    return TRUE;
}

char *backend_clean_field(const char *value) {
    GString *clean = g_string_sized_new(value ? strlen(value) : 0);
    gboolean previous_space = FALSE;
    for (const char *cursor = value ? value : ""; *cursor; cursor++) {
        gunichar character = g_utf8_get_char_validated(cursor, -1);
        if (character == (gunichar)-1 || character == (gunichar)-2) {
            character = '?';
        } else {
            cursor = g_utf8_next_char(cursor) - 1;
        }
        if (character == '\t' || character == '\r' || character == '\n' ||
            g_unichar_isspace(character)) {
            if (!previous_space && clean->len)
                g_string_append_c(clean, ' ');
            previous_space = TRUE;
            continue;
        }
        previous_space = FALSE;
        g_string_append_unichar(clean, character);
    }
    g_strstrip(clean->str);
    clean->len = strlen(clean->str);
    return g_string_free(clean, FALSE);
}

void backend_print_field(const char *value) {
    g_autofree char *clean = backend_clean_field(value);
    fputs(clean, stdout);
}

gboolean backend_spawn_detached(const char *const argv[], GPid *pid,
                                GError **error) {
    return g_spawn_async(NULL, (char **)argv, NULL,
                         G_SPAWN_SEARCH_PATH | G_SPAWN_DO_NOT_REAP_CHILD,
                         NULL, NULL, pid, error);
}

static void detached_child_finished(GPid pid, gint status, gpointer data) {
    (void)status;
    (void)data;
    g_spawn_close_pid(pid);
}

void backend_notify(const char *title, const char *body) {
    if (backend_no_notify()) return;
    const char *program =
        backend_program("ANTO_MENU_NOTIFY_SEND", "notify-send");
    const char *argv[] = {
        program, "--app-name=Anto Control", title ? title : "Menu",
        body ? body : "", NULL,
    };
    g_autoptr(GError) error = NULL;
    GPid pid = 0;
    if (backend_spawn_detached(argv, &pid, &error))
        g_child_watch_add(pid, detached_child_finished, NULL);
}

int backend_error(int status, const char *code, const char *message) {
    g_autofree char *safe_code = backend_clean_field(code ? code : "error");
    g_autofree char *safe_message =
        backend_clean_field(message ? message : "Operazione non riuscita");
    g_printerr("ERROR\t%s\t%s\n", safe_code, safe_message);
    return status;
}

int backend_usage(const char *domain, const char *usage) {
    g_printerr("Uso: anto-menu-backend %s %s\n",
               domain ? domain : "DOMINIO", usage ? usage : "");
    return 2;
}
