#include "backend.h"
#include "service.h"
#include "local_config.h"

#include <stdio.h>
#include <string.h>

static const char default_config[] =
    "# Comando aperto dalla scorciatoia note.\n"
    "# Il valore è eseguito come comando utente esplicito.\n"
    "export ANTO426_NOTES_COMMAND='gnome-text-editor "
    "\"$HOME/Documents/Notes/inbox.md\"'\n";

static char *legacy_config_path(void) {
    const char *override =
        g_getenv("ANTO_MENU_NOTES_LEGACY_CONFIG");
    if (override && *override) return g_strdup(override);
    return g_build_filename(g_get_user_config_dir(), "anto426",
                            "notes.env", NULL);
}

static char *ensure_notes_config(GError **error) {
    const char *relative = "notes/notes.env";
    g_autofree char *path =
        anto_local_config_path(relative, error);
    if (!path) return NULL;
    if (g_file_test(path, G_FILE_TEST_IS_REGULAR))
        return g_steal_pointer(&path);

    int lock = anto_local_config_lock(relative, TRUE, error);
    if (lock < 0) return NULL;
    gboolean ok = TRUE;
    if (!g_file_test(path, G_FILE_TEST_IS_REGULAR)) {
        g_autofree char *legacy = legacy_config_path();
        if (g_file_test(legacy, G_FILE_TEST_IS_REGULAR))
            ok = anto_local_config_migrate_file(
                legacy, relative, FALSE, error);
        else
            ok = anto_local_config_write_text(
                relative, default_config, 0600, error);
    }
    anto_local_config_unlock(lock);
    return ok ? g_steal_pointer(&path) : NULL;
}

static char *notes_directory(void) {
    const char *override = g_getenv("ANTO426_NOTES_DIR");
    if (override && *override) return g_strdup(override);
    const char *documents =
        g_get_user_special_dir(G_USER_DIRECTORY_DOCUMENTS);
    g_autofree char *fallback = NULL;
    if (!documents || !*documents) {
        fallback =
            g_build_filename(g_get_home_dir(), "Documents", NULL);
        documents = fallback;
    }
    return g_build_filename(documents, "Notes", NULL);
}

static char *configured_command_text(const char *content) {
    g_auto(GStrv) lines = g_strsplit(content, "\n", -1);
    for (guint index = 0; lines[index]; index++) {
        char *line = g_strstrip(lines[index]);
        if (g_str_has_prefix(line, "export "))
            line = g_strstrip(line + strlen("export "));
        if (!g_str_has_prefix(line, "ANTO426_NOTES_COMMAND="))
            continue;
        const char *encoded =
            line + strlen("ANTO426_NOTES_COMMAND=");
        g_autoptr(GError) error = NULL;
        char *decoded = g_shell_unquote(encoded, &error);
        return decoded ? decoded : g_strdup(encoded);
    }
    return g_strdup("");
}

static char *configured_command(const char *path) {
    g_autofree char *content = NULL;
    if (!g_file_get_contents(path, &content, NULL, NULL))
        return g_strdup("");
    return configured_command_text(content);
}

static int notes_init(gboolean print_path) {
    g_autoptr(GError) error = NULL;
    g_autofree char *path = ensure_notes_config(&error);
    if (!path)
        return backend_error(
            1, "notes-config",
            error ? error->message :
                    "Configurazione note non disponibile");
    if (print_path) puts(path);
    return 0;
}

static int notes_open(void) {
    g_autoptr(GError) error = NULL;
    g_autofree char *directory = notes_directory();
    g_autofree char *note =
        g_build_filename(directory, "inbox.md", NULL);
    if (backend_dry_run()) {
        g_autofree char *local = anto_local_config_path(
            "notes/notes.env", NULL);
        g_autofree char *legacy = legacy_config_path();
        g_autofree char *command =
            g_file_test(local, G_FILE_TEST_IS_REGULAR)
                ? configured_command(local)
                : g_file_test(legacy, G_FILE_TEST_IS_REGULAR)
                      ? configured_command(legacy)
                      : configured_command_text(default_config);
        g_autofree char *safe = backend_clean_field(command);
        g_print("DRYRUN\tnotes\topen\t%s\t%s\n", note,
                *safe ? safe : "gnome-text-editor");
        return 0;
    }
    g_autofree char *config = ensure_notes_config(&error);
    if (!config)
        return backend_error(
            1, "notes-config",
            error ? error->message :
                    "Configurazione note non disponibile");
    g_autofree char *command = configured_command(config);
    if (g_mkdir_with_parents(directory, 0700) != 0)
        return backend_error(1, "notes-directory",
                             "Directory note non disponibile");
    if (!g_file_test(note, G_FILE_TEST_EXISTS) &&
        !g_file_set_contents(note, "# Inbox\n\n", -1, &error))
        return backend_error(
            1, "notes-file",
            error ? error->message :
                    "Nota iniziale non creata");

    GSubprocess *process = NULL;
    if (*command) {
        const char *argv[] = {"/bin/sh", "-lc", command, NULL};
        process = g_subprocess_newv(
            argv, G_SUBPROCESS_FLAGS_NONE, &error);
    } else {
        const char *editor = backend_program(
            "ANTO_MENU_NOTES_EDITOR", "gnome-text-editor");
        const char *argv[] = {editor, note, NULL};
        process = g_subprocess_newv(
            argv, G_SUBPROCESS_FLAGS_NONE, &error);
    }
    if (!process)
        return backend_error(
            1, "notes-editor",
            error ? error->message :
                    "Editor delle note non avviato");
    g_object_unref(process);
    return 0;
}

int anto_notes_execute(int argc, char **argv) {
    (void)argc;

    extern const BackendService anto_service_notes;
    switch (backend_operation_index(&anto_service_notes, argv[0])) {
        case 0: return notes_open();
        case 1: return notes_init(FALSE);
        case 2: return notes_init(TRUE);
        default: return 2;
    }
}
