#pragma once
/* Page-private state and cross-file contract. Public API stays in menu.h. */
#include "menu.h"

#define NOTIFICATIONS_LIVE_KEY "anto-menu-notifications-live"

typedef struct {
    MenuApp *app;
    AntoQuery *query;
    GSubprocess *toggle_process;
    GCancellable *toggle_cancellable;
    GSubprocess *clear_process;
    GCancellable *clear_cancellable;
    GtkWidget *open_row;
    GtkWidget *open_subtitle;
    GtkWidget *open_badge;
    GtkWidget *toggle_row;
    GtkWidget *toggle_icon;
    GtkWidget *toggle_title;
    GtkWidget *toggle_subtitle;
    GtkWidget *toggle_badge;
    GtkWidget *clear_row;
    GtkWidget *clear_subtitle;
    GtkWidget *clear_badge;
    guint count;
    gboolean enabled;
    gboolean state_known;
    gboolean mounted;
    gboolean pending;
    gboolean toggle_pending;
    gboolean clear_pending;
} NotificationsLive;

typedef struct {
    GWeakRef window;
    GWeakRef source;
    GSubprocess *process;
} NotificationsPending;



gboolean anto_notifications_set_label(GtkWidget *widget,
                                        const char *text);
gboolean anto_notifications_set_search(GtkWidget *row,
                                         const char *text);
GtkWidget *anto_notifications_find(GtkWidget *root, const char *css_class,
                                     gboolean image);
void anto_notifications_live_free(gpointer data);
NotificationsLive *anto_notifications_live_get(MenuApp *app);
void anto_notifications_apply(NotificationsLive *live);
void anto_notifications_pending_free(NotificationsPending *pending);
void anto_notifications_cancel_read(NotificationsLive *live);
void anto_notifications_toggle_finished(GObject *object,
                                          GAsyncResult *result,
                                          gpointer data);
void anto_notifications_toggle(MenuApp *app, gpointer data);
void anto_notifications_open(MenuApp *app, gpointer data);
void anto_notifications_clear_finished(GObject *object,
                                         GAsyncResult *result,
                                         gpointer data);
void anto_notifications_clear(MenuApp *app, gpointer data);
void anto_notifications_finished(GObject *object, GAsyncResult *result,
                                   gpointer data);
void anto_notifications_refresh_start(NotificationsLive *live);
void menu_notifications_live_event(MenuApp *app);
void menu_show_notifications(MenuApp *app);
