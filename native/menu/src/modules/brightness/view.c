#include "internal.h"

gboolean anto_brightness_set_search(GtkWidget *row, const char *text) {
    if (!row) return FALSE;
    g_autofree char *search = g_utf8_strdown(text ? text : "", -1);
    const char *current =
        g_object_get_data(G_OBJECT(row), "menu-search");
    if (g_strcmp0(current, search) == 0) return FALSE;
    g_object_set_data_full(G_OBJECT(row), "menu-search",
                           g_strdup(search), g_free);
    return TRUE;
}

void anto_brightness_profile_rows_set_sensitive(BrightnessLive *live,
                                                  gboolean sensitive) {
    if (!live || !live->mounted ||
        g_strcmp0(live->app->current_page, "brightness") != 0)
        return;
    for (guint i = 0; i < G_N_ELEMENTS(live->profile_rows); i++) {
        if (live->profile_rows[i])
            gtk_widget_set_sensitive(live->profile_rows[i], sensitive);
    }
}

const char *anto_brightness_battery_status_label(const char *status) {
    if (g_ascii_strcasecmp(status, "Charging") == 0) return "in carica";
    if (g_ascii_strcasecmp(status, "Discharging") == 0) return "in uso";
    if (g_ascii_strcasecmp(status, "Full") == 0) return "carica";
    if (g_ascii_strcasecmp(status, "Not charging") == 0) return "collegata";
    return status;
}

const char *anto_brightness_profile_label(const char *profile) {
    if (g_strcmp0(profile, "power-saver") == 0) return "risparmio";
    if (g_strcmp0(profile, "balanced") == 0) return "bilanciato";
    if (g_strcmp0(profile, "performance") == 0) return "prestazioni";
    return profile;
}

GtkWidget *anto_brightness_row_widget(MenuApp *app, int row,
                                        const char *css_class,
                                        gboolean find_scale) {
    GtkListBoxRow *item =
        gtk_list_box_get_row_at_index(GTK_LIST_BOX(app->list), row);
    return item ? anto_brightness_find(GTK_WIDGET(item), css_class, find_scale)
                : NULL;
}

void anto_brightness_render(BrightnessLive *live,
                              const BrightnessSnapshot *snapshot) {
    const BrightnessSnapshot empty = {0};
    if (!snapshot) snapshot = &empty;
    MenuApp *app = live->app;
    double brightness = snapshot->has_brightness ?
                        snapshot->brightness : 1.0;
    g_autofree char *subtitle = anto_brightness_subtitle(snapshot);

    menu_page_begin(app, "display-brightness-symbolic",
                    "Luminosità e batteria", subtitle,
                    "Cerca luminosità o profilo…");
    menu_add_scale(
        app, "display-brightness-symbolic", "Luminosità schermo",
        snapshot->has_brightness ?
            "Regolazione continua del pannello interno" :
            "Il pannello non espone un controllo di luminosità",
        brightness, 1, 100, 1, anto_brightness_set_brightness, NULL, NULL);
    GtkListBoxRow *scale_row =
        gtk_list_box_get_row_at_index(GTK_LIST_BOX(app->list), 0);
    live->scale_row = scale_row ? GTK_WIDGET(scale_row) : NULL;
    live->scale = anto_brightness_row_widget(app, 0, NULL, TRUE);
    live->scale_detail =
        anto_brightness_row_widget(app, 0, "item-subtitle", FALSE);
    if (live->scale) {
        GtkGesture *gesture = gtk_gesture_click_new();
        gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(gesture), 0);
        g_signal_connect(gesture, "pressed",
                         G_CALLBACK(anto_brightness_scale_pressed), live);
        g_signal_connect(gesture, "released",
                         G_CALLBACK(anto_brightness_scale_released), live);
        gtk_widget_add_controller(
            live->scale, GTK_EVENT_CONTROLLER(gesture));
    }

    menu_add_section(app, "PRESET");
    menu_add_shell_item(
        app, "weather-clear-night-symbolic", "Notte · 20%",
        "Comfort al buio e consumo minimo", NULL,
        "brightnessctl set 20% >/dev/null", FALSE);
    menu_add_shell_item(
        app, "display-brightness-low-symbolic", "Interni · 45%",
        "Luce morbida per casa e ufficio", NULL,
        "brightnessctl set 45% >/dev/null", FALSE);
    menu_add_shell_item(
        app, "display-brightness-high-symbolic", "Giorno · 75%",
        "Ambienti molto illuminati", NULL,
        "brightnessctl set 75% >/dev/null", FALSE);
    menu_add_shell_item(
        app, "weather-clear-symbolic", "Massima · 100%",
        "Luminosità piena", NULL,
        "brightnessctl set 100% >/dev/null", FALSE);

    menu_add_section(app, "PROFILO ENERGETICO");
    menu_add_item(
        app, "battery-low-symbolic", "Risparmio energetico",
        "Massima autonomia e temperature contenute",
        " ",
        anto_brightness_set_performance_profile, g_strdup("power-saver"), g_free);
    menu_add_item(
        app, "battery-good-symbolic", "Bilanciato",
        "Profilo quotidiano consigliato",
        " ",
        anto_brightness_set_performance_profile, g_strdup("balanced"), g_free);
    menu_add_item(
        app, "battery-full-symbolic", "Prestazioni",
        "Potenza massima quando serve",
        " ",
        anto_brightness_set_performance_profile, g_strdup("performance"), g_free);
    for (int i = 0; i < 3; i++) {
        GtkListBoxRow *row =
            gtk_list_box_get_row_at_index(GTK_LIST_BOX(app->list), 7 + i);
        live->profile_rows[i] = row ? GTK_WIDGET(row) : NULL;
        live->profile_badges[i] =
            anto_brightness_row_widget(app, 7 + i, "item-badge", FALSE);
    }
    anto_brightness_apply_snapshot(live, snapshot);

    menu_set_footer(
        app,
        "Stato live da UPower, profilo energetico e retroilluminazione");
    live->mounted = TRUE;
}

void menu_show_brightness(MenuApp *app) {
    BrightnessLive *live = anto_brightness_live_get(app);
    if (live->interaction_source) {
        g_source_remove(live->interaction_source);
        live->interaction_source = 0;
    }
    live->mounted = FALSE;
    live->pointer_active = FALSE;
    live->scale_active = FALSE;
    live->scale = NULL;
    live->scale_row = NULL;
    live->scale_detail = NULL;
    memset(live->profile_rows, 0, sizeof(live->profile_rows));
    memset(live->profile_badges, 0, sizeof(live->profile_badges));
    anto_brightness_render(live, live->snapshot);
    anto_brightness_refresh_start(live);
}
