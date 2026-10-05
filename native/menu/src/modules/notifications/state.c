#include "internal.h"

GtkWidget *anto_notifications_find(GtkWidget *root, const char *css_class,
                                     gboolean image) {
    if (!root) return NULL;
    if ((image && GTK_IS_IMAGE(root)) ||
        (!image && gtk_widget_has_css_class(root, css_class)))
        return root;
    for (GtkWidget *child = gtk_widget_get_first_child(root);
         child; child = gtk_widget_get_next_sibling(child)) {
        GtkWidget *match = anto_notifications_find(child, css_class, image);
        if (match) return match;
    }
    return NULL;
}

void anto_notifications_live_free(gpointer data) {
    NotificationsLive *live = data;
    if (!live) return;
    if (live->toggle_cancellable)
        g_cancellable_cancel(live->toggle_cancellable);
    if (live->toggle_process)
        g_subprocess_force_exit(live->toggle_process);
    if (live->clear_cancellable)
        g_cancellable_cancel(live->clear_cancellable);
    if (live->clear_process)
        g_subprocess_force_exit(live->clear_process);
    g_clear_object(&live->toggle_process);
    g_clear_object(&live->toggle_cancellable);
    g_clear_object(&live->clear_process);
    g_clear_object(&live->clear_cancellable);
    anto_query_close(live->query);
    g_clear_object(&live->query);
    g_free(live);
}

NotificationsLive *anto_notifications_live_get(MenuApp *app) {
    NotificationsLive *live =
        g_object_get_data(G_OBJECT(app->window), NOTIFICATIONS_LIVE_KEY);
    if (live) return live;
    live = g_new0(NotificationsLive, 1);
    live->app = app;
    g_object_set_data_full(G_OBJECT(app->window), NOTIFICATIONS_LIVE_KEY,
                           live, anto_notifications_live_free);
    return live;
}

void anto_notifications_apply(NotificationsLive *live) {
    if (!live->mounted ||
        g_strcmp0(live->app->current_page, "notifications") != 0)
        return;

    g_autofree char *page_status = live->state_known
        ? g_strdup_printf(
              "%u %s in cronologia · %s",
              live->count,
              live->count == 1 ? "notifica" : "notifiche",
              live->enabled ? "Non disturbare attivo"
                            : "banner abilitati")
        : g_strdup_printf(
              "%u %s in cronologia · sincronizzazione stato…",
              live->count,
              live->count == 1 ? "notifica" : "notifiche");
    anto_notifications_set_label(live->app->page_subtitle, page_status);

    g_autofree char *history = live->count
        ? g_strdup_printf(
              "%u %s nella cronologia",
              live->count,
              live->count == 1 ? "notifica" : "notifiche")
        : g_strdup("La cronologia è vuota");
    anto_notifications_set_label(live->open_subtitle, history);
    g_autofree char *count_text = g_strdup_printf("%u", live->count);
    anto_notifications_set_label(live->open_badge, count_text);
    if (live->open_badge)
        gtk_widget_set_visible(live->open_badge, live->count > 0);
    if (live->open_row) {
        if (live->count > 0)
            gtk_widget_add_css_class(live->open_row,
                                     "has-notifications");
        else
            gtk_widget_remove_css_class(live->open_row,
                                        "has-notifications");
    }

    if (GTK_IS_IMAGE(live->toggle_icon))
        gtk_image_set_from_icon_name(
            GTK_IMAGE(live->toggle_icon),
            live->enabled ? "notifications-disabled-symbolic"
                          : "notifications-symbolic");
    anto_notifications_set_label(
        live->toggle_title,
        !live->state_known ? "Stato Non disturbare" :
        live->enabled ? "Disattiva Non disturbare"
                      : "Attiva Non disturbare");
    anto_notifications_set_label(
        live->toggle_subtitle,
        !live->state_known ? "Sincronizzazione con SwayNC…" :
        live->enabled ? "Torna a ricevere banner"
                      : "Silenzia temporaneamente i banner");
    anto_notifications_set_label(
        live->toggle_badge, live->enabled ? "ATTIVO" : "");
    if (live->toggle_badge)
        gtk_widget_set_visible(live->toggle_badge,
                               live->state_known && live->enabled);
    if (live->toggle_row) {
        if (live->state_known && live->enabled)
            gtk_widget_add_css_class(live->toggle_row, "active");
        else
            gtk_widget_remove_css_class(live->toggle_row, "active");
        gtk_widget_set_sensitive(live->toggle_row,
                                 live->state_known &&
                                     !live->toggle_pending);
        const char *title = !live->state_known
            ? "Stato Non disturbare"
            : live->enabled
                ? "Disattiva Non disturbare"
                : "Attiva Non disturbare";
        const char *subtitle = !live->state_known
            ? "Sincronizzazione con SwayNC"
            : live->enabled
                ? "Torna a ricevere banner"
                : "Silenzia temporaneamente i banner";
        g_autofree char *combined = g_strdup_printf(
            "%s %s %s", title, subtitle,
            live->enabled ? "attivo" : "spento");
        if (anto_notifications_set_search(live->toggle_row, combined))
            gtk_list_box_invalidate_filter(
                GTK_LIST_BOX(live->app->list));
    }

    g_autofree char *clear_detail = live->count
        ? g_strdup_printf(
              "Rimuove %u %s dalla cronologia",
              live->count,
              live->count == 1 ? "notifica" : "notifiche")
        : g_strdup("Nessuna notifica da rimuovere");
    anto_notifications_set_label(live->clear_subtitle, clear_detail);
    anto_notifications_set_label(live->clear_badge, count_text);
    if (live->clear_badge)
        gtk_widget_set_visible(live->clear_badge, live->count > 0);
    if (live->clear_row)
        gtk_widget_set_sensitive(
            live->clear_row,
            live->count > 0 && !live->clear_pending);
}

void anto_notifications_pending_free(NotificationsPending *pending) {
    if (!pending) return;
    g_weak_ref_clear(&pending->window);
    g_weak_ref_clear(&pending->source);
    g_clear_object(&pending->process);
    g_free(pending);
}

void anto_notifications_open(MenuApp *app, gpointer data) {
    (void)data;
    const char *argv[] = {
        "/usr/bin/timeout", "--kill-after=1", "3",
        "swaync-client", "-op", "-sw", NULL,
    };
    menu_spawn(app, argv, TRUE);
}

void anto_notifications_clear_finished(GObject *object,
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
    if (live && live->clear_process == G_SUBPROCESS(object)) {
        g_clear_object(&live->clear_process);
        g_clear_object(&live->clear_cancellable);
        live->clear_pending = FALSE;
        if (ok) {
            live->count = 0;
        } else {
            if (error_text) g_strstrip(error_text);
            menu_notify(
                "Notifiche",
                error_text && *error_text ? error_text :
                error ? error->message :
                "Impossibile svuotare la cronologia");
        }
        anto_notifications_apply(live);
        anto_notifications_refresh_start(live);
    }
    g_clear_object(&window);
    g_free(output);
    g_free(error_text);
    anto_notifications_pending_free(pending);
}

void anto_notifications_clear(MenuApp *app, gpointer data) {
    NotificationsLive *live = data;
    if (!live || live->clear_process || live->clear_pending ||
        live->count == 0)
        return;

    const char *argv[] = {
        "/usr/bin/timeout", "--kill-after=1", "3",
        "swaync-client", "-C", "-sw", NULL,
    };
    g_autoptr(GError) error = NULL;
    live->clear_process = g_subprocess_newv(
        argv, G_SUBPROCESS_FLAGS_STDOUT_PIPE |
                  G_SUBPROCESS_FLAGS_STDERR_PIPE,
        &error);
    if (!live->clear_process) {
        menu_notify("Notifiche",
                    error ? error->message :
                    "Impossibile svuotare la cronologia");
        return;
    }
    live->clear_pending = TRUE;
    anto_notifications_apply(live);
    live->clear_cancellable = g_cancellable_new();
    NotificationsPending *pending = g_new0(NotificationsPending, 1);
    g_weak_ref_init(&pending->window, G_OBJECT(app->window));
    g_weak_ref_init(&pending->source,
                    live->clear_row
                        ? G_OBJECT(live->clear_row)
                        : NULL);
    pending->process = g_object_ref(live->clear_process);
    g_subprocess_communicate_utf8_async(
        live->clear_process, NULL, live->clear_cancellable,
        anto_notifications_clear_finished, pending);
}

static void notifications_received(const char *output, const GError *error, gboolean changed, gpointer data) {
    (void)changed;
    NotificationsLive *live = data;
    if (live->app->closing) return;
        if (!error && output) {

            g_auto(GStrv) fields = g_strsplit(output, "\t", 2);
            if (fields[0] &&
                (g_strcmp0(fields[0], "true") == 0 ||
                 g_strcmp0(fields[0], "false") == 0)) {
                live->enabled =
                    g_strcmp0(fields[0], "true") == 0;
                live->state_known = TRUE;
            }
            if (fields[1] && *fields[1]) {
                char *end = NULL;
                guint64 value = g_ascii_strtoull(fields[1], &end, 10);
                if (end && *end == '\0')
                    live->count = (guint)MIN(value, G_MAXUINT);
            }
            anto_notifications_apply(live);
        }
}


void anto_notifications_refresh_start(NotificationsLive *live) {
    if (!live->query) {
        g_autofree char *backend = menu_backend_path();
        const char *argv[] = {backend, "notifications", "snapshot", NULL};
        live->query = anto_query_new(G_OBJECT(live->app->window), argv, 5, notifications_received, live);
    }
    anto_query_request(live->query);
}


void menu_notifications_live_event(MenuApp *app) {
    if (!app || !app->window ||
        g_strcmp0(app->current_page, "notifications") != 0)
        return;
    anto_notifications_refresh_start(anto_notifications_live_get(app));
}
