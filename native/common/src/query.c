#include "query.h"
#include <string.h>
#include <signal.h>
struct _AntoQuery {
    GObject parent;
    GWeakRef owner;
    char **argv;
    char *cache;
    GSubprocess *process;
    GCancellable *cancel;
    AntoQueryResult result;
    gpointer data;
    gboolean pending, closed;
};
G_DEFINE_FINAL_TYPE(AntoQuery, anto_query, G_TYPE_OBJECT)

void anto_query_close(AntoQuery *self) {
    if (!self || self->closed) return;
    self->closed = TRUE;
    self->pending = FALSE;
    self->result = NULL;
    self->data = NULL;
    if (self->cancel) g_cancellable_cancel(self->cancel);
    if (self->process) g_subprocess_send_signal(self->process, SIGTERM);
}
static void dispose(GObject *object) {
    AntoQuery *self = ANTO_QUERY(object);
    anto_query_close(self);
    g_clear_object(&self->process);
    g_clear_object(&self->cancel);
    G_OBJECT_CLASS(anto_query_parent_class)->dispose(object);
}
static void finalize(GObject *object) {
    AntoQuery *self = ANTO_QUERY(object);
    g_weak_ref_clear(&self->owner);
    g_strfreev(self->argv);
    g_free(self->cache);
    G_OBJECT_CLASS(anto_query_parent_class)->finalize(object);
}
static void anto_query_class_init(AntoQueryClass *klass) {
    G_OBJECT_CLASS(klass)->dispose = dispose;
    G_OBJECT_CLASS(klass)->finalize = finalize;
}
static void anto_query_init(AntoQuery *self) { g_weak_ref_init(&self->owner, NULL); }
AntoQuery *anto_query_new(GObject *owner, const char *const argv[], guint timeout_seconds,
                          AntoQueryResult result, gpointer data) {
    g_return_val_if_fail(G_IS_OBJECT(owner) && argv && argv[0], NULL);
    AntoQuery *self = g_object_new(ANTO_TYPE_QUERY, NULL);
    g_weak_ref_set(&self->owner, owner);
    self->result = result;
    self->data = data;
    guint count = g_strv_length((char **)argv);
    self->argv = g_new0(char *, count + 6);
    self->argv[0] = g_strdup("/usr/bin/timeout");
    self->argv[1] = g_strdup("--signal=TERM");
    self->argv[2] = g_strdup("--kill-after=1");
    self->argv[3] = g_strdup_printf("%u", MAX(timeout_seconds, 1u));
    for (guint i = 0; i < count; i++) self->argv[i + 4] = g_strdup(argv[i]);
    return self;
}
static void completed(GObject *object, GAsyncResult *result, gpointer data) {
    AntoQuery *self = data;
    g_autofree char *output = NULL, *diagnostic = NULL;
    g_autoptr(GError) error = NULL;
    gboolean ok = g_subprocess_communicate_utf8_finish(G_SUBPROCESS(object), result, &output, &diagnostic, &error);
    if (ok && !g_subprocess_get_successful(G_SUBPROCESS(object))) {
        ok = FALSE;
        g_set_error(&error, G_IO_ERROR, G_IO_ERROR_FAILED, "Servizio terminato con codice %d",
                    g_subprocess_get_if_exited(G_SUBPROCESS(object)) ? g_subprocess_get_exit_status(G_SUBPROCESS(object)) : -1);
    }
    g_clear_object(&self->process);
    g_clear_object(&self->cancel);
    gboolean repeat = self->pending;
    self->pending = FALSE;
    g_autoptr(GObject) owner = g_weak_ref_get(&self->owner);
    if (!self->closed && owner) {
        gboolean changed = ok && g_strcmp0(self->cache, output) != 0;
        if (ok) { g_free(self->cache); self->cache = g_strdup(output ? output : ""); }
        if (self->result) self->result(ok ? self->cache : NULL, error, changed, self->data);
        /* A callback may already have started the refresh that was pending. */
        if (repeat && !self->closed && !self->process) anto_query_request(self);
    } else anto_query_close(self);
    g_object_unref(self);
}
void anto_query_request(AntoQuery *self) {
    if (!self || self->closed) return;
    if (self->process) { self->pending = TRUE; return; }
    g_autoptr(GObject) owner = g_weak_ref_get(&self->owner);
    if (!owner) { anto_query_close(self); return; }
    g_autoptr(GError) error = NULL;
    self->process = g_subprocess_newv((const char *const *)self->argv,
        G_SUBPROCESS_FLAGS_STDOUT_PIPE | G_SUBPROCESS_FLAGS_STDERR_PIPE, &error);
    if (!self->process) {
        if (self->result) self->result(NULL, error, FALSE, self->data);
        return;
    }
    self->cancel = g_cancellable_new();
    g_subprocess_communicate_utf8_async(self->process, NULL, self->cancel, completed, g_object_ref(self));
}
const char *anto_query_cached(AntoQuery *self) { return self ? self->cache : NULL; }
gboolean anto_query_busy(AntoQuery *self) { return self && self->process != NULL; }
