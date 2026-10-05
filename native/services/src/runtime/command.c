#include "backend.h"
#include <stdio.h>

static int exit_status(GSubprocess *process) {
    return g_subprocess_get_if_exited(process) ? g_subprocess_get_exit_status(process)
         : g_subprocess_get_if_signaled(process) ? 128 + g_subprocess_get_term_sig(process) : 1;
}
static char *bytes_to_text(GBytes *bytes) {
    gsize length = 0;
    const char *text = bytes ? g_bytes_get_data(bytes, &length) : NULL;
    /* GLib represents an empty byte buffer with a NULL data pointer. */
    return length ? g_utf8_make_valid(text, length) : g_strdup("");
}
void backend_command_clear(BackendCommand *result) {
    if (!result) return;
    g_clear_pointer(&result->stdout_text, g_free);
    g_clear_pointer(&result->stderr_text, g_free);
    result->status = -1;
    result->started = FALSE;
}
BackendCommand backend_command_run_bytes(const char *const argv[], GBytes *input, GBytes **output) {
    BackendCommand result = {.status = 127};
    if (output) *output = NULL;
    g_autoptr(GError) error = NULL;
    GSubprocessFlags flags = G_SUBPROCESS_FLAGS_STDOUT_PIPE | G_SUBPROCESS_FLAGS_STDERR_PIPE;
    if (input) flags |= G_SUBPROCESS_FLAGS_STDIN_PIPE;
    g_autoptr(GSubprocess) process = g_subprocess_newv(argv, flags, &error);
    if (!process) { result.stderr_text = g_strdup(error->message); return result; }
    result.started = TRUE;
    g_autoptr(GBytes) bytes = NULL, diagnostic = NULL;
    if (!g_subprocess_communicate(process, input, NULL, &bytes, &diagnostic, &error)) {
        g_subprocess_force_exit(process);
        result.status = 1;
        result.stderr_text = g_strdup(error->message);
        return result;
    }
    result.status = exit_status(process);
    result.stderr_text = bytes_to_text(diagnostic);
    if (output) *output = g_steal_pointer(&bytes);
    return result;
}
BackendCommand backend_command_run(const char *const argv[], const char *input) {
    g_autoptr(GBytes) in = input ? g_bytes_new(input, strlen(input)) : NULL;
    g_autoptr(GBytes) out = NULL;
    BackendCommand result = backend_command_run_bytes(argv, in, &out);
    result.stdout_text = bytes_to_text(out);
    return result;
}
BackendCommand backend_run_program(const char *environment, const char *fallback, const char *const arguments[]) {
    guint count = arguments ? g_strv_length((char **)arguments) : 0;
    const char **argv = g_new0(const char *, count + 2);
    argv[0] = backend_program(environment, fallback);
    for (guint i = 0; i < count; i++) argv[i + 1] = arguments[i];
    BackendCommand result = backend_command_run(argv, NULL);
    g_free(argv);
    return result;
}
char *backend_first_line(const char *text) {
    const char *end = text ? strchr(text, '\n') : NULL;
    char *line = end ? g_strndup(text, end - text) : g_strdup(text ? text : "");
    return g_strstrip(line);
}
int backend_command_forward(const char *const argv[], const char *input) {
    BackendCommand result = backend_command_run(argv, input);
    if (result.stdout_text) fputs(result.stdout_text, stdout);
    if (result.stderr_text) fputs(result.stderr_text, stderr);
    int status = result.status;
    backend_command_clear(&result);
    return status;
}
