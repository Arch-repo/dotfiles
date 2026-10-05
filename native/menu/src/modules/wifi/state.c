#include "internal.h"
#include "primitives.h"

char *anto_wifi_network_action_path(void) {
    return menu_backend_path();
}

gboolean anto_wifi_text_is_true(const char *text) {
    return g_strcmp0(text, "true") == 0 || g_strcmp0(text, "yes") == 0;
}

const char *anto_wifi_present_text(const char *text) {
    return text && *text && g_strcmp0(text, "--") != 0 ? text : NULL;
}

char *anto_wifi_unescape_field(const char *text) {
    GString *decoded = g_string_new(NULL);
    gboolean escaped = FALSE;
    for (const char *cursor = text ? text : ""; *cursor; cursor++) {
        if (!escaped && *cursor == '\\') {
            escaped = TRUE;
            continue;
        }
        if (escaped) {
            switch (*cursor) {
                case 't': g_string_append_c(decoded, '\t'); break;
                case 'r': g_string_append_c(decoded, '\r'); break;
                case 'n': g_string_append_c(decoded, '\n'); break;
                default: g_string_append_c(decoded, *cursor); break;
            }
            escaped = FALSE;
        } else {
            g_string_append_c(decoded, *cursor);
        }
    }
    if (escaped) g_string_append_c(decoded, '\\');
    return g_string_free(decoded, FALSE);
}

int anto_wifi_parse_number(const char *text, int fallback) {
    char *end = NULL;
    long value = strtol(text ? text : "", &end, 10);
    return end && end != text ? (int)CLAMP(value, G_MININT, G_MAXINT) : fallback;
}

void anto_wifi_network_free(gpointer data) {
    WifiNetwork *network = data;
    if (!network) return;
    g_free(network->ssid);
    g_free(network->security);
    g_free(network->bssid);
    g_free(network);
}

void anto_wifi_snapshot_free(WifiSnapshot *snapshot) {
    if (!snapshot) return;
    g_free(snapshot->state);
    g_free(snapshot->device);
    g_free(snapshot->ssid);
    g_free(snapshot->ipv4);
    g_free(snapshot->gateway);
    g_free(snapshot->dns);
    g_free(snapshot->connectivity);
    g_ptr_array_free(snapshot->networks, TRUE);
    g_free(snapshot);
}

WifiSnapshot *anto_wifi_snapshot_new(void) {
    WifiSnapshot *snapshot = g_new0(WifiSnapshot, 1);
    snapshot->networks = g_ptr_array_new_with_free_func(anto_wifi_network_free);
    return snapshot;
}

WifiSnapshot *anto_wifi_snapshot_parse(const char *output) {
    WifiSnapshot *snapshot = anto_wifi_snapshot_new();
    g_auto(GStrv) lines = g_strsplit(output ? output : "", "\n", -1);
    GHashTable *seen = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);

    for (guint index = 0; lines[index]; index++) {
        if (!*lines[index]) continue;
        g_auto(GStrv) fields = g_strsplit(lines[index], "\t", -1);
        if (g_strcmp0(fields[0], "STATUS") == 0 && fields[9]) {
            snapshot->parsed = TRUE;
            snapshot->available = anto_wifi_text_is_true(fields[1]);
            snapshot->radio_enabled = g_strcmp0(fields[2], "enabled") == 0;
            snapshot->state = anto_wifi_unescape_field(fields[3]);
            snapshot->device = anto_wifi_unescape_field(fields[4]);
            snapshot->ssid = anto_wifi_unescape_field(fields[5]);
            snapshot->ipv4 = anto_wifi_unescape_field(fields[6]);
            snapshot->gateway = anto_wifi_unescape_field(fields[7]);
            snapshot->dns = anto_wifi_unescape_field(fields[8]);
            snapshot->connectivity = anto_wifi_unescape_field(fields[9]);
            continue;
        }
        if (g_strcmp0(fields[0], "NETWORK") != 0 || !fields[8]) continue;
        g_autofree char *ssid = anto_wifi_unescape_field(fields[1]);
        g_autofree char *bssid = anto_wifi_unescape_field(fields[6]);
        g_autofree char *key = anto_wifi_network_key(ssid, bssid);
        if (!anto_wifi_present_text(ssid) || g_hash_table_contains(seen, key)) continue;
        g_hash_table_add(seen, g_steal_pointer(&key));

        WifiNetwork *network = g_new0(WifiNetwork, 1);
        network->ssid = g_strdup(ssid);
        network->signal = CLAMP(anto_wifi_parse_number(fields[2], 0), 0, 100);
        network->security = anto_wifi_unescape_field(fields[3]);
        network->active = anto_wifi_text_is_true(fields[4]);
        network->saved = anto_wifi_text_is_true(fields[5]);
        network->bssid = g_strdup(bssid);
        network->frequency = anto_wifi_parse_number(fields[7], 0);
        network->channel = anto_wifi_parse_number(fields[8], 0);
        g_ptr_array_add(snapshot->networks, network);
    }
    g_hash_table_destroy(seen);
    return snapshot;
}

void anto_wifi_runtime_free(gpointer data) {
    WifiRuntime *runtime = data;
    if (!runtime) return;
    if (runtime->debounce_source)
        g_source_remove(runtime->debounce_source);
    anto_wifi_snapshot_free(runtime->snapshot);
    anto_query_close(runtime->query);
    g_clear_object(&runtime->query);
    g_free(runtime);
}

WifiRuntime *anto_wifi_runtime_get(MenuApp *app) {
    WifiRuntime *runtime =
        g_object_get_data(G_OBJECT(app->window), WIFI_RUNTIME_KEY);
    if (runtime) return runtime;
    runtime = g_new0(WifiRuntime, 1);
    runtime->app = app;
    g_object_set_data_full(G_OBJECT(app->window), WIFI_RUNTIME_KEY,
                           runtime, anto_wifi_runtime_free);
    return runtime;
}

const char *anto_wifi_network_band(const WifiNetwork *network) {
    if (network->frequency >= 5925) return "6 GHz";
    if (network->frequency >= 4900) return "5 GHz";
    if (network->frequency > 0) return "2.4 GHz";
    return NULL;
}

void anto_wifi_action_set(WifiAction *action, const char *operation,
                            const char *argument) {
    if (!action) return;
    g_free(action->operation);
    g_free(action->argument);
    action->operation = g_strdup(operation);
    action->argument = g_strdup(argument);
}

void anto_wifi_request_free(WifiRequest *request) {
    if (!request) return;
    g_weak_ref_clear(&request->window);
    g_weak_ref_clear(&request->source);
    g_weak_ref_clear(&request->status);
    g_clear_object(&request->process);
    if (request->input) {
        memset(request->input, 0, strlen(request->input));
        g_free(request->input);
    }
    g_free(request);
}

void anto_wifi_open_editor(GtkButton *button, gpointer data) {
    (void)button;
    MenuApp *app = data;
    const char *argv[] = {"nm-connection-editor", NULL};
    menu_spawn(app, argv, FALSE);
}

void anto_wifi_password_free(gpointer data, GClosure *closure) {
    (void)closure;
    WifiPassword *password = data;
    if (!password) return;
    g_free(password->ssid);
    g_free(password);
}

GtkWidget *anto_wifi_connect_button(MenuApp *app,
                                      const WifiNetwork *network) {
    const gboolean open = !anto_wifi_present_text(network->security);
    const char *label = network->active ? "Connessa" :
                        network->saved ? "Connetti" :
                        open ? "Connetti" : "Password";
    const char *operation = network->saved ? "connect-saved" :
                            open ? "connect-open" : "password";
    GtkWidget *button = anto_ui_action(label, NULL, NULL);
    gtk_widget_add_css_class(button, "wifi-action-chip");
    if (!network->active) gtk_widget_add_css_class(button, "primary");
    gtk_widget_set_sensitive(button, !network->active);

    WifiAction *action = g_new0(WifiAction, 1);
    action->app = app;
    action->operation = g_strdup(operation);
    action->argument = g_strdup(network->ssid);
    g_object_set_data(G_OBJECT(button), "anto-wifi-action", action);
    g_signal_connect_data(button, "clicked", G_CALLBACK(anto_wifi_connect_clicked),
                          action, anto_wifi_action_free, 0);
    return button;
}

void anto_wifi_group_free(gpointer data) {
    g_free(data);
}

void anto_wifi_card_ref_free(gpointer data) {
    WifiCardRef *reference = data;
    if (!reference) return;
    g_free(reference->key);
    g_free(reference->search_text);
    g_free(reference);
}

void anto_wifi_view_free(gpointer data) {
    WifiView *view = data;
    if (!view) return;
    if (view->scroll_restore_id)
        g_source_remove(view->scroll_restore_id);
    g_ptr_array_free(view->groups, TRUE);
    g_ptr_array_free(view->cards, TRUE);
    g_free(view);
}

guint anto_wifi_network_group(const WifiNetwork *network) {
    if (network->active) return WIFI_GROUP_CONNECTED;
    if (network->saved) return WIFI_GROUP_SAVED;
    return WIFI_GROUP_AVAILABLE;
}

void anto_wifi_view_apply_snapshot(WifiView *view,
                                     const WifiSnapshot *snapshot) {
    GtkAdjustment *adjustment =
        gtk_scrolled_window_get_vadjustment(
            GTK_SCROLLED_WINDOW(view->scroll));
    double scroll_value = gtk_adjustment_get_value(adjustment);

    anto_wifi_hero_update(view, snapshot);
    anto_wifi_cards_reconcile(view, snapshot);
    view->networks_enabled =
        snapshot->available && snapshot->radio_enabled;
    gtk_widget_set_visible(view->unavailable_empty,
                           !snapshot->available);
    gtk_widget_set_visible(view->radio_empty,
                           snapshot->available &&
                           !snapshot->radio_enabled);
    gtk_widget_set_visible(view->networks_empty,
                           view->networks_enabled &&
                           snapshot->networks->len == 0);

    const char *query =
        gtk_editable_get_text(GTK_EDITABLE(view->app->search));
    anto_wifi_search(view->app, query, view);
    anto_wifi_schedule_scroll_restore(view, scroll_value);
}

void anto_wifi_render_loading(MenuApp *app) {
    anto_wifi_render(app, NULL);
}

static void wifi_received(const char *output, const GError *error, gboolean changed, gpointer data) {
    (void)changed;
    WifiRuntime *runtime = data;
    if (runtime->app->closing) return;
    WifiSnapshot *snapshot = output && !error ?
                             anto_wifi_snapshot_parse(output) : NULL;
    if (snapshot && snapshot->parsed) {
        gboolean on_page =
            g_strcmp0(runtime->app->current_page, "wifi") == 0;
        anto_wifi_snapshot_free(runtime->snapshot);
        runtime->snapshot = snapshot;
        WifiView *view = on_page ?
            anto_wifi_current_view(runtime->app) : NULL;
        if (view)
            anto_wifi_view_apply_snapshot(view, runtime->snapshot);
    } else {
        anto_wifi_snapshot_free(snapshot);
        if (g_strcmp0(runtime->app->current_page, "wifi") == 0 &&
            !runtime->snapshot) {
            WifiSnapshot *fallback = anto_wifi_snapshot_new();
            fallback->parsed = TRUE;
            fallback->state = g_strdup("unavailable");
            fallback->connectivity = g_strdup("unknown");
            runtime->snapshot = fallback;
            WifiView *view = anto_wifi_current_view(runtime->app);
            if (view)
                anto_wifi_view_apply_snapshot(view, runtime->snapshot);
        }
    }

}


void anto_wifi_start_snapshot(MenuApp *app) {
    WifiRuntime *runtime = anto_wifi_runtime_get(app);
    if (!runtime->query) {
        g_autofree char *backend = anto_wifi_network_action_path();
        const char *argv[] = {backend, "network", "snapshot", NULL};
        runtime->query = anto_query_new(G_OBJECT(runtime->app->window), argv, 8, wifi_received, runtime);
    }
    anto_query_request(runtime->query);
}


gboolean anto_wifi_debounced_refresh(gpointer data) {
    WifiRuntime *runtime = data;
    runtime->debounce_source = 0;
    if (g_strcmp0(runtime->app->current_page, "wifi") == 0)
        anto_wifi_start_snapshot(runtime->app);
    return G_SOURCE_REMOVE;
}

void menu_wifi_live_event(MenuApp *app) {
    if (!app || !app->window ||
        g_strcmp0(app->current_page, "wifi") != 0)
        return;
    WifiRuntime *runtime = anto_wifi_runtime_get(app);
    if (runtime->debounce_source) return;
    runtime->debounce_source =
        g_timeout_add(180, anto_wifi_debounced_refresh, runtime);
}
const char *anto_wifi_signal_icon(int signal) {
    if (signal >= 75) return "network-wireless-signal-excellent-symbolic";
    if (signal >= 50) return "network-wireless-signal-good-symbolic";
    if (signal >= 25) return "network-wireless-signal-ok-symbolic";
    return "network-wireless-signal-weak-symbolic";
}

const char *anto_wifi_connectivity_label(const char *state) {
    if (g_strcmp0(state, "full") == 0) return "Internet disponibile";
    if (g_strcmp0(state, "limited") == 0) return "Connettività limitata";
    if (g_strcmp0(state, "portal") == 0) return "Accesso richiesto";
    if (g_strcmp0(state, "none") == 0) return "Senza Internet";
    return "Connettività in verifica";
}

const char *anto_wifi_connection_state_label(const char *state) {
    if (g_strcmp0(state, "connected") == 0) return "Connessa";
    if (g_strcmp0(state, "connecting") == 0) return "Connessione…";
    if (g_strcmp0(state, "disconnected") == 0) return "Non connessa";
    if (g_strcmp0(state, "unavailable") == 0) return "Non disponibile";
    return anto_wifi_present_text(state) ? state : "In attesa";
}
