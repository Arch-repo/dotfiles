#include "internal.h"

void anto_notifications_cancel_read(NotificationsLive *live) {
    anto_query_close(live->query);
    g_clear_object(&live->query);
}


void anto_notifications_toggle_finished(GObject *object,
                                          GAsyncResult *result,
                                          gpointer data) {
    NotificationsPending *pending = data;
    char *output = NULL;
    char *error_text = NULL;
    g_autoptr(GError) error = NULL;
    gboolean ok = g_subprocess_communicate_utf8_finish(
        G_SUBPROCESS(object), result, &output, &error_text, &error);
    ok = ok && g_subprocess_get_successful(G_SUBPROCESS(object));
    GtkWidget *window = g_weak_ref_get(&pending->window);
    NotificationsLive *live = window
        ? g_object_get_data(G_OBJECT(window), NOTIFICATIONS_LIVE_KEY)
        : NULL;
    if (live && live->toggle_process == G_SUBPROCESS(object)) {
        g_clear_object(&live->toggle_process);
        g_clear_object(&live->toggle_cancellable);
        live->toggle_pending = FALSE;
        GObject *source = g_weak_ref_get(&pending->source);
        if (source && GTK_IS_WIDGET(source) &&
            GTK_WIDGET(source) == live->toggle_row)
            gtk_widget_set_sensitive(GTK_WIDGET(source), TRUE);
        g_clear_object(&source);
        if (ok && output) {
            g_strstrip(output);
            if (g_strcmp0(output, "true") == 0 ||
                g_strcmp0(output, "false") == 0) {
                live->enabled = g_strcmp0(output, "true") == 0;
                live->state_known = TRUE;
            }
        } else {
            if (error_text) g_strstrip(error_text);
            menu_notify(
                "Notifiche",
                error_text && *error_text ? error_text :
                error ? error->message :
                "Impossibile modificare Non disturbare");
        }
        anto_notifications_apply(live);
        anto_notifications_refresh_start(live);
    }
    g_clear_object(&window);
    g_free(output);
    g_free(error_text);
    anto_notifications_pending_free(pending);
}

void anto_notifications_toggle(MenuApp *app, gpointer data) {
    NotificationsLive *live = data;
    if (live->toggle_process || live->toggle_pending) return;
    /* An older status query could otherwise land after the toggle and
     * temporarily restore the previous value. */
    anto_notifications_cancel_read(live);
    const char *argv[] = {
        "/usr/bin/timeout", "--kill-after=1", "3",
        "swaync-client", "-d", "-sw", NULL,
    };
    g_autoptr(GError) error = NULL;
    live->toggle_process = g_subprocess_newv(
        argv, G_SUBPROCESS_FLAGS_STDOUT_PIPE |
                  G_SUBPROCESS_FLAGS_STDERR_PIPE,
        &error);
    if (!live->toggle_process) {
        menu_notify("Notifiche",
                    error ? error->message :
                    "Impossibile modificare Non disturbare");
        anto_notifications_refresh_start(live);
        return;
    }
    live->toggle_pending = TRUE;
    if (live->toggle_row)
        gtk_widget_set_sensitive(live->toggle_row, FALSE);
    live->toggle_cancellable = g_cancellable_new();
    NotificationsPending *pending = g_new0(NotificationsPending, 1);
    g_weak_ref_init(&pending->window, G_OBJECT(app->window));
    g_weak_ref_init(&pending->source,
                    live->toggle_row
                        ? G_OBJECT(live->toggle_row)
                        : NULL);
    pending->process = g_object_ref(live->toggle_process);
    g_subprocess_communicate_utf8_async(
        live->toggle_process, NULL, live->toggle_cancellable,
        anto_notifications_toggle_finished, pending);
}
