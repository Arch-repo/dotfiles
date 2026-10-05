#include "frontend.h"
#include <gdk/wayland/gdkwayland.h>

typedef struct { GWeakRef window; } FrontendClaim;

static void claim_free(gpointer data) {
    FrontendClaim *claim = data;
    g_weak_ref_clear(&claim->window);
    g_free(claim);
}

static void release_name(gpointer data) {
    g_bus_unown_name(GPOINTER_TO_UINT(data));
}

static void acquired(GDBusConnection *connection, const char *name, gpointer data) {
    (void)connection; (void)name;
    FrontendClaim *claim = data;
    g_autoptr(GtkWindow) window = g_weak_ref_get(&claim->window);
    if (window) gtk_window_present(window);
}

static void lost(GDBusConnection *connection, const char *name, gpointer data) {
    (void)name;
    FrontendClaim *claim = data;
    g_autoptr(GtkWindow) window = g_weak_ref_get(&claim->window);
    /* A missing session bus must not make a usable standalone window vanish. */
    if (!connection || !window) return;
    gtk_window_close(window);
}

void anto_frontend_present(GtkWindow *window) {
    g_return_if_fail(GTK_IS_WINDOW(window));
    GtkApplication *application = gtk_window_get_application(window);
    GDBusConnection *connection = application &&
        g_application_get_is_registered(G_APPLICATION(application))
        ? g_application_get_dbus_connection(G_APPLICATION(application)) : NULL;
    if (!GDK_IS_WAYLAND_DISPLAY(gtk_widget_get_display(GTK_WIDGET(window))) || !connection) {
        gtk_window_present(window);
        return;
    }
    /* Reclaim on activation too, so the latest user intent wins. Unowning the
     * old claim removes its callbacks before installing a fresh weak owner. */
    g_object_set_data(G_OBJECT(window), "anto-frontend-claim", NULL);
    FrontendClaim *claim = g_new0(FrontendClaim, 1);
    g_weak_ref_init(&claim->window, window);
    guint owner = g_bus_own_name_on_connection(connection, "com.anto426.Shell.Frontend",
        G_BUS_NAME_OWNER_FLAGS_ALLOW_REPLACEMENT | G_BUS_NAME_OWNER_FLAGS_REPLACE |
        G_BUS_NAME_OWNER_FLAGS_DO_NOT_QUEUE, acquired, lost, claim, claim_free);
    g_object_set_data_full(G_OBJECT(window), "anto-frontend-claim",
                          GUINT_TO_POINTER(owner), release_name);
}
