#include "internal.h"

gboolean anto_notifications_set_label(GtkWidget *widget,
                                        const char *text) {
    if (!GTK_IS_LABEL(widget)) return FALSE;
    const char *current = gtk_label_get_text(GTK_LABEL(widget));
    if (g_strcmp0(current, text ? text : "") == 0) return FALSE;
    gtk_label_set_text(GTK_LABEL(widget), text ? text : "");
    return TRUE;
}

gboolean anto_notifications_set_search(GtkWidget *row,
                                         const char *text) {
    if (!row) return FALSE;
    g_autofree char *search = g_utf8_strdown(text ? text : "", -1);
    const char *current =
        g_object_get_data(G_OBJECT(row), "menu-search");
    if (g_strcmp0(current, search) == 0) return FALSE;
    g_object_set_data_full(G_OBJECT(row), "menu-search",
                           g_strdup(search), g_free);
    return TRUE;
}

void menu_show_notifications(MenuApp *app) {
    NotificationsLive *live = anto_notifications_live_get(app);
    live->mounted = FALSE;
    live->open_row = NULL;
    live->open_subtitle = NULL;
    live->open_badge = NULL;
    live->toggle_row = NULL;
    live->toggle_icon = NULL;
    live->toggle_title = NULL;
    live->toggle_subtitle = NULL;
    live->toggle_badge = NULL;
    live->clear_row = NULL;
    live->clear_subtitle = NULL;
    live->clear_badge = NULL;

    menu_page_begin(
        app, "preferences-system-notifications-symbolic", "Notifiche",
        "Cronologia e modalità Non disturbare",
        "Cerca un controllo notifiche…");
    menu_add_item(
        app, "preferences-system-notifications-symbolic",
        "Apri centro notifiche", "Cronologia completa di SwayNC",
        "0", anto_notifications_open, NULL, NULL);
    GtkListBoxRow *open =
        gtk_list_box_get_row_at_index(GTK_LIST_BOX(app->list), 0);
    if (open) {
        live->open_row = GTK_WIDGET(open);
        gtk_widget_add_css_class(live->open_row, "notifications-hero-row");
        live->open_subtitle =
            anto_notifications_find(live->open_row, "item-subtitle", FALSE);
        live->open_badge =
            anto_notifications_find(live->open_row, "item-badge", FALSE);
    }

    menu_add_item(
        app, "notifications-disabled-symbolic",
        "Attiva Non disturbare", "Silenzia temporaneamente i banner",
        "ATTIVO", anto_notifications_toggle, live, NULL);
    GtkListBoxRow *toggle =
        gtk_list_box_get_row_at_index(GTK_LIST_BOX(app->list), 1);
    if (toggle) {
        live->toggle_row = GTK_WIDGET(toggle);
        gtk_widget_add_css_class(live->toggle_row,
                                 "notifications-toggle-row");
        live->toggle_icon =
            anto_notifications_find(GTK_WIDGET(toggle), NULL, TRUE);
        live->toggle_title =
            anto_notifications_find(GTK_WIDGET(toggle), "item-title", FALSE);
        live->toggle_subtitle =
            anto_notifications_find(GTK_WIDGET(toggle), "item-subtitle", FALSE);
        live->toggle_badge =
            anto_notifications_find(GTK_WIDGET(toggle), "item-badge", FALSE);
    }

    menu_add_item(
        app, "edit-clear-all-symbolic", "Cancella notifiche",
        "Nessuna notifica da rimuovere", "0",
        anto_notifications_clear, live, NULL);
    GtkListBoxRow *clear =
        gtk_list_box_get_row_at_index(GTK_LIST_BOX(app->list), 2);
    if (clear) {
        live->clear_row = GTK_WIDGET(clear);
        gtk_widget_add_css_class(live->clear_row,
                                 "notifications-clear-row");
        live->clear_subtitle =
            anto_notifications_find(live->clear_row, "item-subtitle", FALSE);
        live->clear_badge =
            anto_notifications_find(live->clear_row, "item-badge", FALSE);
    }

    menu_set_footer(
        app,
        "Stato aggiornato dagli eventi SwayNC  ·  nessun refresh della pagina");
    live->mounted = TRUE;
    anto_notifications_apply(live);
    anto_notifications_refresh_start(live);
}
