#include "internal.h"

char *anto_wifi_network_key(const char *ssid, const char *bssid) {
    g_autofree char *ssid_folded =
        g_utf8_casefold(anto_wifi_present_text(ssid) ? ssid : "", -1);
    g_autofree char *bssid_folded =
        g_ascii_strdown(anto_wifi_present_text(bssid) ? bssid : "", -1);
    return g_strdup_printf("%s\x1f%s", ssid_folded, bssid_folded);
}

void anto_wifi_action_free(gpointer data, GClosure *closure) {
    (void)closure;
    WifiAction *action = data;
    if (!action) return;
    g_free(action->operation);
    g_free(action->argument);
    g_free(action);
}

void anto_wifi_action_finished(GObject *source, GAsyncResult *result,
                                 gpointer data) {
    WifiRequest *request = data;
    char *output = NULL;
    char *error_text = NULL;
    g_autoptr(GError) error = NULL;
    gboolean communicated = g_subprocess_communicate_utf8_finish(
        G_SUBPROCESS(source), result, &output, &error_text, &error);
    gboolean ok = communicated &&
                  g_subprocess_get_successful(G_SUBPROCESS(source));

    GtkWidget *window = g_weak_ref_get(&request->window);
    GtkWidget *button = g_weak_ref_get(&request->source);
    GtkWidget *status = g_weak_ref_get(&request->status);
    if (button) {
        g_object_set_data(G_OBJECT(button), "wifi-operation-pending", NULL);
        if (!ok) gtk_widget_set_sensitive(button, TRUE);
    }
    if (!ok) {
        if (error_text) g_strstrip(error_text);
        const char *message = error_text && *error_text ? error_text :
                              error ? error->message :
                              "NetworkManager non ha completato l’azione";
        menu_notify("Wi‑Fi", message);
        if (status) gtk_label_set_text(GTK_LABEL(status), message);
    }

    if (window) {
        WifiRuntime *runtime =
            g_object_get_data(G_OBJECT(window), WIFI_RUNTIME_KEY);
        MenuApp *app = runtime ? runtime->app : NULL;
        if (app && ok && request->return_to_wifi &&
            g_strcmp0(app->current_page, "wifi-password") == 0) {
            menu_back(app);
        } else if (app) {
            menu_wifi_live_event(app);
        }
    }

    g_clear_object(&window);
    g_clear_object(&button);
    g_clear_object(&status);
    g_free(output);
    g_free(error_text);
    anto_wifi_request_free(request);
}

void anto_wifi_run_action(MenuApp *app, const char *operation,
                            const char *argument, const char *input,
                            GtkWidget *source, GtkWidget *status,
                            gboolean return_to_wifi) {
    if (source &&
        g_object_get_data(G_OBJECT(source), "wifi-operation-pending"))
        return;
    g_autofree char *script = anto_wifi_network_action_path();
    const char *argv[] = {
        script, "network", operation, argument, NULL,
    };
    g_autoptr(GError) error = NULL;
    GSubprocess *process = g_subprocess_newv(
        argv, G_SUBPROCESS_FLAGS_STDIN_PIPE |
                  G_SUBPROCESS_FLAGS_STDOUT_PIPE |
                  G_SUBPROCESS_FLAGS_STDERR_PIPE,
        &error);
    if (!process) {
        const char *message = error ? error->message :
                              "Impossibile avviare il controllo Wi‑Fi";
        menu_notify("Wi‑Fi", message);
        if (status) gtk_label_set_text(GTK_LABEL(status), message);
        return;
    }

    if (source) {
        g_object_set_data(G_OBJECT(source), "wifi-operation-pending",
                          GINT_TO_POINTER(1));
        gtk_widget_set_sensitive(source, FALSE);
    }
    if (status) gtk_label_set_text(GTK_LABEL(status), "Connessione in corso…");
    WifiRequest *request = g_new0(WifiRequest, 1);
    g_weak_ref_init(&request->window, app->window);
    g_weak_ref_init(&request->source, source);
    g_weak_ref_init(&request->status, status);
    request->process = process;
    request->input = g_strdup(input);
    request->return_to_wifi = return_to_wifi;
    g_subprocess_communicate_utf8_async(
        process, request->input, NULL, anto_wifi_action_finished, request);
}

void anto_wifi_action_clicked(GtkButton *button, gpointer data) {
    WifiAction *action = data;
    anto_wifi_run_action(action->app, action->operation, action->argument, NULL,
                    GTK_WIDGET(button), NULL, FALSE);
}

void anto_wifi_radio_changed(GObject *object, GParamSpec *spec,
                               gpointer data) {
    (void)spec;
    MenuApp *app = data;
    const char *target =
        gtk_switch_get_active(GTK_SWITCH(object)) ? "on" : "off";
    anto_wifi_run_action(app, "radio", target, NULL,
                    GTK_WIDGET(object), NULL, FALSE);
}

void anto_wifi_password_submit(GtkButton *button, gpointer data) {
    WifiPassword *prompt = data;
    const char *password =
        gtk_editable_get_text(GTK_EDITABLE(prompt->entry));
    if (!password || !*password) {
        gtk_label_set_text(GTK_LABEL(prompt->status),
                           "Inserisci la password della rete.");
        gtk_widget_grab_focus(prompt->entry);
        return;
    }
    g_autofree char *input = g_strdup_printf("%s\n", password);
    anto_wifi_run_action(prompt->app, "connect-secure", prompt->ssid, input,
                    GTK_WIDGET(button), prompt->status, TRUE);
}

void anto_wifi_password_cancel(GtkButton *button, gpointer data) {
    (void)button;
    menu_back(data);
}

void anto_wifi_connect_clicked(GtkButton *button, gpointer data) {
    WifiAction *action = data;
    if (g_strcmp0(action->operation, "password") == 0) {
        anto_wifi_show_password(action->app, action->argument);
        return;
    }
    anto_wifi_run_action(action->app, action->operation, action->argument, NULL,
                    GTK_WIDGET(button), NULL, FALSE);
}
