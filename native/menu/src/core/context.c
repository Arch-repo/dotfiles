#include "menu.h"
#include "query.h"
#include <string.h>
static void status_card_set_state(GtkWidget *card, const char *state_class) {
    static const char *const states[] = {
        "status-accent", "status-green", "status-warn", "status-danger",
        "status-offline", NULL,
    };
    const char *current =
        g_object_get_data(G_OBJECT(card), "context-state-class");
    if (g_strcmp0(current, state_class) == 0) return;
    for (guint i = 0; states[i]; i++)
        gtk_widget_remove_css_class(card, states[i]);
    if (state_class) gtk_widget_add_css_class(card, state_class);
    g_object_set_data_full(G_OBJECT(card), "context-state-class",
                           g_strdup(state_class), g_free);
}

static void status_card_set(GtkWidget *card, GtkWidget *image, GtkWidget *value,
                            const char *icon, const char *text,
                            const char *state_class) {
    const char *current_icon =
        g_object_get_data(G_OBJECT(image), "context-icon-name");
    if (g_strcmp0(current_icon, icon) != 0) {
        gtk_image_set_from_icon_name(GTK_IMAGE(image), icon);
        g_object_set_data_full(G_OBJECT(image), "context-icon-name",
                               g_strdup(icon), g_free);
    }
    if (g_strcmp0(gtk_label_get_text(GTK_LABEL(value)), text) != 0)
        gtk_label_set_text(GTK_LABEL(value), text);
    status_card_set_state(card, state_class);
}

static gboolean update_clock(gpointer data) {
    MenuApp *app = data;
    if (app->closing) {
        app->clock_source = 0;
        return G_SOURCE_REMOVE;
    }
    static const char *const days[] = {
        "", "Lunedì", "Martedì", "Mercoledì", "Giovedì", "Venerdì", "Sabato", "Domenica",
    };
    static const char *const months[] = {
        "", "Gennaio", "Febbraio", "Marzo", "Aprile", "Maggio", "Giugno",
        "Luglio", "Agosto", "Settembre", "Ottobre", "Novembre", "Dicembre",
    };
    g_autoptr(GDateTime) now = g_date_time_new_now_local();
    g_autofree char *time = g_date_time_format(now, "%H:%M");
    int day_of_week = g_date_time_get_day_of_week(now);
    int month = g_date_time_get_month(now);
    g_autofree char *date = g_strdup_printf("%s · %02d %s", days[day_of_week],
                                            g_date_time_get_day_of_month(now), months[month]);
    if (g_strcmp0(gtk_label_get_text(GTK_LABEL(app->context_time)),
                  time) != 0)
        gtk_label_set_text(GTK_LABEL(app->context_time), time);
    if (g_strcmp0(gtk_label_get_text(GTK_LABEL(app->context_date)),
                  date) != 0)
        gtk_label_set_text(GTK_LABEL(app->context_date), date);
    menu_calendar_clock_tick(app);
    return G_SOURCE_CONTINUE;
}


static const char *field(GHashTable *fields, const char *key, const char *fallback) {
    const char *text = g_hash_table_lookup(fields, key);
    return text && *text ? text : fallback;
}
static void summary_received(const char *output, const GError *error, gboolean changed, gpointer data) {
    MenuApp *app = data;
    if (app->closing) return;
    if (error || !output) {
        /* Keep a good snapshot on transient failure; finish the initial
         * loading state even when a provider cannot return data. */
        if (app->desktop_snapshot) return;
        output = "volume\tNon disponibile\nnetwork\tNon disponibile\n"
                 "bluetooth\tNon disponibile\nbrightness\tNon disponibile\n"
                 "displays\tNon disponibile\nbattery\tNon disponibile\n";
    } else if (!changed) return;
    g_free(app->desktop_snapshot);
    app->desktop_snapshot = g_strdup(output);
    g_autoptr(GHashTable) fields = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
    g_auto(GStrv) lines = g_strsplit(output, "\n", -1);
    for (guint i = 0; lines[i]; i++) {
        char *tab = strchr(lines[i], '\t');
        if (!tab) continue;
        *tab = 0;
        g_hash_table_replace(fields, g_strdup(lines[i]), g_strdup(tab + 1));
    }
    const char *network = field(fields, "network", "Disconnessa");
    const char *audio = field(fields, "volume", "Non disponibile");
    const char *display = field(fields, "displays", "Nessuno attivo");
    const char *battery = field(fields, "battery", "Non rilevata");
    gboolean online = g_strcmp0(network, "Disconnessa") != 0 &&
                      g_strcmp0(network, "Non disponibile") != 0;
    gboolean muted = strstr(audio, "MUTE") != NULL;
    status_card_set(app->context_network_card, app->context_network_icon, app->context_network_value,
                    online ? "network-wireless-symbolic" : "network-offline-symbolic", network,
                    online ? "status-accent" : "status-offline");
    status_card_set(app->context_audio_card, app->context_audio_icon, app->context_audio_value,
                    muted ? "audio-volume-muted-symbolic" : "audio-volume-high-symbolic", audio,
                    muted ? "status-offline" : "status-accent");
    status_card_set(app->context_display_card, app->context_display_icon, app->context_display_value,
                    "video-display-symbolic", display, "status-accent");
    status_card_set(app->context_battery_card, app->context_battery_icon, app->context_battery_value,
                    "battery-good-symbolic", battery, "status-green");
    menu_system_accept(app, output);
}
void menu_context_status_refresh(MenuApp *app) {
    if (!app || app->closing) return;
    if (!app->summary) {
        g_autofree char *backend = menu_backend_path();
        const char *argv[] = {backend, "system", "snapshot", NULL};
        app->summary = anto_query_new(G_OBJECT(app->window), argv, 8, summary_received, app);
    }
    anto_query_request(app->summary);
}
static gboolean fallback(gpointer data) {
    menu_context_status_refresh(data);
    return G_SOURCE_CONTINUE;
}
void menu_context_start(MenuApp *app) {
    update_clock(app);
    menu_context_status_refresh(app);
    app->clock_source = g_timeout_add_seconds(1, update_clock, app);
    app->status_source = g_timeout_add_seconds(10, fallback, app);
}
