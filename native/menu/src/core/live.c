#include "menu.h"

#include <gio/gunixsocketaddress.h>
#include <glib/gstdio.h>
#include <string.h>

enum {
    LIVE_CONTEXT = 1u << 0,
    LIVE_WIFI = 1u << 1,
    LIVE_BLUETOOTH = 1u << 2,
    LIVE_DISPLAY = 1u << 3,
    LIVE_CALENDAR = 1u << 4,
    LIVE_AUDIO = 1u << 5,
    LIVE_POWER = 1u << 6,
    LIVE_WINDOWS = 1u << 7,
    LIVE_KEYBOARD = 1u << 8,
    LIVE_NOTIFICATIONS = 1u << 9,
    LIVE_LAUNCHER = 1u << 10,
    LIVE_CLIPBOARD = 1u << 11,
};

struct _MenuLive {
    gint refs;
    MenuApp *app;
    gboolean stopping;
    guint pending;
    guint dispatch_source;
    guint page_tick_source;
    guint audio_restart_source;
    guint audio_page_source;
    guint hypr_restart_source;
    guint brightness_tick_source;
    guint audio_retry_seconds;
    guint hypr_retry_seconds;
    gboolean hypr_connecting;

    GCancellable *cancel;
    GDBusConnection *system_bus;
    guint network_subscription;
    guint network_topology_subscription;
    guint wireless_topology_subscription;
    guint bluez_subscription;
    guint upower_subscription;
    guint power_profiles_subscription;

    GDBusConnection *session_bus;
    guint mpris_subscription;
    guint mpris_owner_subscription;
    guint swaync_subscription;

    GSubprocess *audio_process;
    GDataInputStream *audio_stream;

    GSocketConnection *hypr_connection;
    GDataInputStream *hypr_stream;

    GFileMonitor *calendar_monitor;
    GFileMonitor *clipboard_monitor;
    GAppInfoMonitor *app_info_monitor;
    gulong app_info_handler;
};

static void live_queue(MenuLive *live, guint events);
static void audio_listener_start(MenuLive *live);
static void hypr_listener_start(MenuLive *live);

static MenuLive *live_ref(MenuLive *live) {
    g_atomic_int_inc(&live->refs);
    return live;
}

static void live_unref(gpointer data) {
    MenuLive *live = data;
    if (live && g_atomic_int_dec_and_test(&live->refs))
        g_free(live);
}

static void live_closure_unref(gpointer data, GClosure *closure) {
    (void)closure;
    live_unref(data);
}

static gboolean refresh_live_metrics(gpointer data) {
    MenuLive *live = data;
    if (live->stopping) return G_SOURCE_REMOVE;
    /* CPU, memory and temperatures do not expose a useful change signal.
     * Poll only while their page is actually visible. */
    menu_hardware_live_event(live->app);
    menu_background_live_event(live->app);
    /* fcitx process state has no Hyprland event; the module is a no-op
     * unless the keyboard page is visible. */
    menu_keyboard_live_event(live->app);
    /* BoltDB does not expose D-Bus notifications and mmap-backed writes are
     * not reported consistently by every filesystem. Keep this inexpensive
     * page-only fallback in addition to the directory monitor below. */
    menu_clipboard_live_event(live->app);
    return G_SOURCE_CONTINUE;
}

static gboolean refresh_brightness_metric(gpointer data) {
    MenuLive *live = data;
    if (live->stopping) return G_SOURCE_REMOVE;
    /* Backlight sysfs attributes do not reliably emit inotify events on every
     * driver. The module ignores this tick unless its page is visible. */
    menu_brightness_live_event(live->app);
    return G_SOURCE_CONTINUE;
}

static gboolean dispatch_live_events(gpointer data) {
    MenuLive *live = data;
    guint events = live->pending;
    live->pending = 0;
    live->dispatch_source = 0;
    if (live->stopping) return G_SOURCE_REMOVE;

    if (events & LIVE_CONTEXT) {
        menu_context_status_refresh(live->app);
    }
    if (events & LIVE_WIFI)
        menu_wifi_live_event(live->app);
    if (events & LIVE_BLUETOOTH)
        menu_bluetooth_live_event(live->app);
    if (events & LIVE_DISPLAY)
        menu_display_live_event(live->app);
    if (events & LIVE_CALENDAR)
        menu_calendar_live_event(live->app);
    if (events & LIVE_AUDIO) {
        menu_audio_live_event(live->app);
        if (!(events & LIVE_BLUETOOTH)) menu_bluetooth_live_event(live->app);
    }
    if (events & LIVE_POWER) {
        menu_brightness_live_event(live->app);
        menu_hardware_live_event(live->app);
    }
    if (events & LIVE_NOTIFICATIONS)
        menu_notifications_live_event(live->app);
    if (events & LIVE_LAUNCHER)
        menu_launcher_live_event(live->app);
    if (events & LIVE_WINDOWS) {
        menu_floating_live_event(live->app);
        menu_background_live_event(live->app);
    }
    if (events & LIVE_KEYBOARD)
        menu_keyboard_live_event(live->app);
    if (events & LIVE_CLIPBOARD)
        menu_clipboard_live_event(live->app);
    /* Pages that have no stable value bindings intentionally do not rebuild
     * here. A listener may update only widgets owned by its page module. */
    return G_SOURCE_REMOVE;
}

static void live_queue(MenuLive *live, guint events) {
    if (!live || live->stopping) return;
    live->pending |= events;
    /* Fixed-window debounce: event storms are coalesced but cannot postpone a
     * refresh forever (important for fluctuating Wi-Fi RSSI and BlueZ scans). */
    if (!live->dispatch_source)
        live->dispatch_source = g_timeout_add_full(
            G_PRIORITY_DEFAULT, 500, dispatch_live_events, live_ref(live),
            live_unref);
}

static void network_changed(GDBusConnection *connection, const char *sender,
                            const char *path, const char *interface,
                            const char *signal, GVariant *parameters,
                            gpointer data) {
    (void)connection;
    (void)sender;
    (void)path;
    (void)interface;
    (void)signal;
    (void)parameters;
    live_queue(data, LIVE_CONTEXT | LIVE_WIFI);
}

static void bluez_changed(GDBusConnection *connection, const char *sender,
                          const char *path, const char *interface,
                          const char *signal, GVariant *parameters,
                          gpointer data) {
    (void)connection;
    (void)sender;
    (void)path;
    (void)interface;
    (void)signal;
    (void)parameters;
    live_queue(data, LIVE_CONTEXT | LIVE_BLUETOOTH);
}

static void upower_changed(GDBusConnection *connection, const char *sender,
                           const char *path, const char *interface,
                           const char *signal, GVariant *parameters,
                           gpointer data) {
    (void)connection;
    (void)sender;
    (void)path;
    (void)interface;
    (void)signal;
    (void)parameters;
    live_queue(data, LIVE_CONTEXT | LIVE_POWER);
}

static void power_profiles_changed(GDBusConnection *connection,
                                   const char *sender, const char *path,
                                   const char *interface, const char *signal,
                                   GVariant *parameters, gpointer data) {
    (void)connection;
    (void)sender;
    (void)path;
    (void)interface;
    (void)signal;
    (void)parameters;
    live_queue(data, LIVE_CONTEXT | LIVE_POWER);
}

static void mpris_changed(GDBusConnection *connection, const char *sender,
                          const char *path, const char *interface,
                          const char *signal, GVariant *parameters,
                          gpointer data) {
    (void)connection;
    (void)sender;
    (void)path;
    (void)interface;
    (void)signal;
    const char *changed_interface = "";
    if (parameters && g_variant_n_children(parameters) > 0)
        g_variant_get_child(parameters, 0, "&s", &changed_interface);
    if (g_str_has_prefix(changed_interface,
                         "org.mpris.MediaPlayer2"))
        live_queue(data, LIVE_AUDIO);
}

static void mpris_owner_changed(GDBusConnection *connection,
                                const char *sender, const char *path,
                                const char *interface, const char *signal,
                                GVariant *parameters, gpointer data) {
    (void)connection;
    (void)sender;
    (void)path;
    (void)interface;
    (void)signal;
    const char *name = "";
    const char *old_owner = "";
    const char *new_owner = "";
    if (parameters)
        g_variant_get(parameters, "(&s&s&s)", &name, &old_owner, &new_owner);
    (void)old_owner;
    (void)new_owner;
    if (g_str_has_prefix(name, "org.mpris.MediaPlayer2."))
        live_queue(data, LIVE_AUDIO);
}

static void swaync_changed(GDBusConnection *connection, const char *sender,
                           const char *path, const char *interface,
                           const char *signal, GVariant *parameters,
                           gpointer data) {
    (void)connection;
    (void)sender;
    (void)path;
    (void)interface;
    (void)signal;
    (void)parameters;
    live_queue(data, LIVE_NOTIFICATIONS);
}

static void applications_changed(GAppInfoMonitor *monitor, gpointer data) {
    (void)monitor;
    live_queue(data, LIVE_LAUNCHER);
}

static void system_bus_ready(GObject *object, GAsyncResult *result,
                             gpointer data) {
    (void)object;
    MenuLive *live = data;
    g_autoptr(GError) error = NULL;
    GDBusConnection *connection = g_bus_get_finish(result, &error);
    if (live->stopping || !connection) {
        g_clear_object(&connection);
        live_unref(live);
        return;
    }
    live->system_bus = connection;

    live->network_subscription = g_dbus_connection_signal_subscribe(
        live->system_bus, "org.freedesktop.NetworkManager",
        "org.freedesktop.DBus.Properties", "PropertiesChanged", NULL, NULL,
        G_DBUS_SIGNAL_FLAGS_NONE, network_changed, live_ref(live), live_unref);
    live->network_topology_subscription =
        g_dbus_connection_signal_subscribe(
            live->system_bus, "org.freedesktop.NetworkManager",
            "org.freedesktop.NetworkManager", NULL, NULL, NULL,
            G_DBUS_SIGNAL_FLAGS_NONE, network_changed, live_ref(live),
            live_unref);
    live->wireless_topology_subscription =
        g_dbus_connection_signal_subscribe(
            live->system_bus, "org.freedesktop.NetworkManager",
            "org.freedesktop.NetworkManager.Device.Wireless", NULL,
            NULL, NULL, G_DBUS_SIGNAL_FLAGS_NONE, network_changed,
            live_ref(live), live_unref);
    live->bluez_subscription = g_dbus_connection_signal_subscribe(
        live->system_bus, "org.bluez", NULL, NULL, NULL, NULL,
        G_DBUS_SIGNAL_FLAGS_NONE,
        bluez_changed, live_ref(live), live_unref);
    live->upower_subscription = g_dbus_connection_signal_subscribe(
        live->system_bus, "org.freedesktop.UPower",
        "org.freedesktop.DBus.Properties", "PropertiesChanged", NULL, NULL,
        G_DBUS_SIGNAL_FLAGS_NONE, upower_changed, live_ref(live), live_unref);
    live->power_profiles_subscription =
        g_dbus_connection_signal_subscribe(
            live->system_bus, "net.hadess.PowerProfiles",
            "org.freedesktop.DBus.Properties", "PropertiesChanged",
            "/net/hadess/PowerProfiles", NULL, G_DBUS_SIGNAL_FLAGS_NONE,
            power_profiles_changed, live_ref(live), live_unref);
    live_unref(live);
}

static void session_bus_ready(GObject *object, GAsyncResult *result,
                              gpointer data) {
    (void)object;
    MenuLive *live = data;
    g_autoptr(GError) error = NULL;
    GDBusConnection *connection = g_bus_get_finish(result, &error);
    if (live->stopping || !connection) {
        g_clear_object(&connection);
        live_unref(live);
        return;
    }
    live->session_bus = connection;

    live->mpris_subscription = g_dbus_connection_signal_subscribe(
        live->session_bus, NULL, "org.freedesktop.DBus.Properties",
        "PropertiesChanged", "/org/mpris/MediaPlayer2", NULL,
        G_DBUS_SIGNAL_FLAGS_NONE, mpris_changed, live_ref(live), live_unref);
    live->mpris_owner_subscription = g_dbus_connection_signal_subscribe(
        live->session_bus, "org.freedesktop.DBus", "org.freedesktop.DBus",
        "NameOwnerChanged", "/org/freedesktop/DBus", NULL,
        G_DBUS_SIGNAL_FLAGS_NONE, mpris_owner_changed, live_ref(live),
        live_unref);
    live->swaync_subscription = g_dbus_connection_signal_subscribe(
        live->session_bus, "org.erikreider.swaync",
        "org.erikreider.swaync.cc", NULL,
        "/org/erikreider/swaync/cc", NULL, G_DBUS_SIGNAL_FLAGS_NONE,
        swaync_changed, live_ref(live), live_unref);
    live_unref(live);
}

static void system_listeners_start(MenuLive *live) {
    g_bus_get(G_BUS_TYPE_SYSTEM, live->cancel, system_bus_ready,
              live_ref(live));
    g_bus_get(G_BUS_TYPE_SESSION, live->cancel, session_bus_ready,
              live_ref(live));
}

static void audio_read_next(MenuLive *live);

static gboolean audio_page_refresh(gpointer data) {
    MenuLive *live = data;
    live->audio_page_source = 0;
    if (!live->stopping)
        menu_audio_live_event(live->app);
    return G_SOURCE_REMOVE;
}

static void audio_page_queue(MenuLive *live) {
    if (live->stopping) return;
    /* pactl emits one event for nearly every slider step. A trailing
     * debounce coalesces value updates while the user is still dragging. */
    if (live->audio_page_source)
        g_source_remove(live->audio_page_source);
    live->audio_page_source = g_timeout_add_full(
        G_PRIORITY_DEFAULT, 650, audio_page_refresh, live_ref(live),
        live_unref);
}

static gboolean audio_listener_restart(gpointer data) {
    MenuLive *live = data;
    live->audio_restart_source = 0;
    if (!live->stopping) audio_listener_start(live);
    return G_SOURCE_REMOVE;
}

static void audio_listener_schedule_restart(MenuLive *live) {
    if (live->stopping || live->audio_restart_source) return;
    guint delay = MAX(live->audio_retry_seconds, 3u);
    live->audio_retry_seconds = MIN(delay * 2, 30u);
    live->audio_restart_source = g_timeout_add_seconds_full(
        G_PRIORITY_DEFAULT, delay, audio_listener_restart, live_ref(live),
        live_unref);
}

static void audio_line_ready(GObject *object, GAsyncResult *result,
                             gpointer data) {
    MenuLive *live = data;
    gsize length = 0;
    g_autoptr(GError) error = NULL;
    char *line = g_data_input_stream_read_line_finish(
        G_DATA_INPUT_STREAM(object), result, &length, &error);
    (void)length;
    if (!line) {
        audio_listener_schedule_restart(live);
        live_unref(live);
        return;
    }
    live->audio_retry_seconds = 3;
    if (strstr(line, "sink") || strstr(line, "source") ||
        strstr(line, "card") || strstr(line, "server")) {
        live_queue(live, LIVE_CONTEXT);
        audio_page_queue(live);
    }
    g_free(line);
    audio_read_next(live);
    live_unref(live);
}

static void audio_read_next(MenuLive *live) {
    if (!live->stopping && live->audio_stream)
        g_data_input_stream_read_line_async(
            live->audio_stream, G_PRIORITY_DEFAULT, live->cancel,
            audio_line_ready, live_ref(live));
}

static void audio_listener_start(MenuLive *live) {
    if (live->stopping) return;
    g_clear_object(&live->audio_stream);
    if (live->audio_process) {
        g_subprocess_force_exit(live->audio_process);
        g_clear_object(&live->audio_process);
    }

    g_autoptr(GError) error = NULL;
    live->audio_process = g_subprocess_new(
        G_SUBPROCESS_FLAGS_STDOUT_PIPE | G_SUBPROCESS_FLAGS_STDERR_SILENCE,
        &error, "/usr/bin/env", "LC_ALL=C", "/usr/bin/pactl", "subscribe",
        NULL);
    if (!live->audio_process) {
        audio_listener_schedule_restart(live);
        return;
    }
    live->audio_stream = g_data_input_stream_new(
        g_subprocess_get_stdout_pipe(live->audio_process));
    audio_read_next(live);
}

static void hypr_read_next(MenuLive *live);

static gboolean hypr_listener_restart(gpointer data) {
    MenuLive *live = data;
    live->hypr_restart_source = 0;
    if (!live->stopping) hypr_listener_start(live);
    return G_SOURCE_REMOVE;
}

static void hypr_listener_schedule_restart(MenuLive *live) {
    if (live->stopping || live->hypr_restart_source ||
        live->hypr_connecting)
        return;
    guint delay = MAX(live->hypr_retry_seconds, 3u);
    live->hypr_retry_seconds = MIN(delay * 2, 30u);
    live->hypr_restart_source = g_timeout_add_seconds_full(
        G_PRIORITY_DEFAULT, delay, hypr_listener_restart, live_ref(live),
        live_unref);
}

static void hypr_line_ready(GObject *object, GAsyncResult *result,
                            gpointer data) {
    MenuLive *live = data;
    gsize length = 0;
    g_autoptr(GError) error = NULL;
    char *line = g_data_input_stream_read_line_finish(
        G_DATA_INPUT_STREAM(object), result, &length, &error);
    (void)length;
    if (!line) {
        hypr_listener_schedule_restart(live);
        live_unref(live);
        return;
    }
    live->hypr_retry_seconds = 3;
    if (g_str_has_prefix(line, "monitoradded") ||
        g_str_has_prefix(line, "monitorremoved") ||
        g_str_has_prefix(line, "configreloaded") ||
        g_str_has_prefix(line, "focusedmon") ||
        g_str_has_prefix(line, "moveworkspace"))
        live_queue(live, LIVE_CONTEXT | LIVE_DISPLAY);
    if (g_str_has_prefix(line, "openwindow") ||
        g_str_has_prefix(line, "closewindow") ||
        g_str_has_prefix(line, "movewindow") ||
        g_str_has_prefix(line, "activewindow") ||
        g_str_has_prefix(line, "windowtitle") ||
        g_str_has_prefix(line, "changefloatingmode") ||
        g_str_has_prefix(line, "fullscreen") ||
        g_str_has_prefix(line, "minimize"))
        live_queue(live, LIVE_WINDOWS);
    if (g_str_has_prefix(line, "activelayout"))
        live_queue(live, LIVE_KEYBOARD);
    g_free(line);
    hypr_read_next(live);
    live_unref(live);
}

static void hypr_read_next(MenuLive *live) {
    if (!live->stopping && live->hypr_stream)
        g_data_input_stream_read_line_async(
            live->hypr_stream, G_PRIORITY_DEFAULT, live->cancel,
            hypr_line_ready, live_ref(live));
}

static void hypr_connected(GObject *object, GAsyncResult *result,
                           gpointer data) {
    MenuLive *live = data;
    g_autoptr(GError) error = NULL;
    GSocketConnection *connection = g_socket_client_connect_finish(
        G_SOCKET_CLIENT(object), result, &error);
    live->hypr_connecting = FALSE;
    if (live->stopping) {
        g_clear_object(&connection);
        live_unref(live);
        return;
    }
    if (!connection) {
        hypr_listener_schedule_restart(live);
        live_unref(live);
        return;
    }
    live->hypr_retry_seconds = 3;
    live->hypr_connection = connection;
    live->hypr_stream = g_data_input_stream_new(
        g_io_stream_get_input_stream(G_IO_STREAM(connection)));
    hypr_read_next(live);
    live_unref(live);
}

static void hypr_listener_start(MenuLive *live) {
    if (live->stopping || live->hypr_connecting) return;
    g_clear_object(&live->hypr_stream);
    if (live->hypr_connection) {
        g_io_stream_close(G_IO_STREAM(live->hypr_connection), NULL, NULL);
        g_clear_object(&live->hypr_connection);
    }

    const char *signature = g_getenv("HYPRLAND_INSTANCE_SIGNATURE");
    if (!signature || !*signature) return;
    g_autofree char *path = g_build_filename(
        g_get_user_runtime_dir(), "hypr", signature, ".socket2.sock", NULL);
    g_autoptr(GSocketClient) client = g_socket_client_new();
    g_autoptr(GSocketAddress) address = g_unix_socket_address_new(path);
    live->hypr_connecting = TRUE;
    g_socket_client_connect_async(
        client, G_SOCKET_CONNECTABLE(address), live->cancel, hypr_connected,
        live_ref(live));
}

static void calendar_changed(GFileMonitor *monitor, GFile *file,
                             GFile *other_file, GFileMonitorEvent event,
                             gpointer data) {
    (void)monitor;
    (void)event;
    g_autofree char *name = file ? g_file_get_basename(file) : NULL;
    g_autofree char *other_name =
        other_file ? g_file_get_basename(other_file) : NULL;
    if (g_strcmp0(name, "events.json") == 0 ||
        g_strcmp0(name, "google_events.json") == 0 ||
        g_strcmp0(other_name, "events.json") == 0 ||
        g_strcmp0(other_name, "google_events.json") == 0)
        live_queue(data, LIVE_CALENDAR);
}

static void calendar_listener_start(MenuLive *live) {
    g_autofree char *path = g_build_filename(
        g_get_user_data_dir(), "anto426", "calendar", NULL);
    if (g_mkdir_with_parents(path, 0700) != 0) return;
    g_autoptr(GFile) directory = g_file_new_for_path(path);
    g_autoptr(GError) error = NULL;
    live->calendar_monitor = g_file_monitor_directory(
        directory, G_FILE_MONITOR_WATCH_MOVES, live->cancel, &error);
    if (live->calendar_monitor)
        g_signal_connect_data(live->calendar_monitor, "changed",
                              G_CALLBACK(calendar_changed), live_ref(live),
                              live_closure_unref, 0);
}

static void clipboard_changed(GFileMonitor *monitor, GFile *file,
                              GFile *other_file,
                              GFileMonitorEvent event, gpointer data) {
    (void)monitor;
    (void)event;
    g_autofree char *name =
        file ? g_file_get_basename(file) : NULL;
    g_autofree char *other_name =
        other_file ? g_file_get_basename(other_file) : NULL;
    if (g_strcmp0(name, "db") == 0 ||
        g_strcmp0(other_name, "db") == 0)
        live_queue(data, LIVE_CLIPBOARD);
}

static void clipboard_listener_start(MenuLive *live) {
    g_autofree char *path =
        g_build_filename(g_get_user_cache_dir(), "cliphist", NULL);
    if (g_mkdir_with_parents(path, 0700) != 0) return;
    g_autoptr(GFile) directory = g_file_new_for_path(path);
    g_autoptr(GError) error = NULL;
    live->clipboard_monitor = g_file_monitor_directory(
        directory, G_FILE_MONITOR_WATCH_MOVES, live->cancel, &error);
    if (live->clipboard_monitor)
        g_signal_connect_data(live->clipboard_monitor, "changed",
                              G_CALLBACK(clipboard_changed),
                              live_ref(live), live_closure_unref, 0);
}

static void applications_listener_start(MenuLive *live) {
    live->app_info_monitor = g_object_ref(g_app_info_monitor_get());
    live->app_info_handler = g_signal_connect_data(
        live->app_info_monitor, "changed", G_CALLBACK(applications_changed),
        live_ref(live), live_closure_unref, 0);
}

void menu_live_start(MenuApp *app) {
    if (!app || app->live) return;
    MenuLive *live = g_new0(MenuLive, 1);
    live->refs = 1;
    live->app = app;
    live->audio_retry_seconds = 3;
    live->hypr_retry_seconds = 3;
    live->cancel = g_cancellable_new();
    app->live = live;

    system_listeners_start(live);
    audio_listener_start(live);
    hypr_listener_start(live);
    calendar_listener_start(live);
    clipboard_listener_start(live);
    applications_listener_start(live);
    live->page_tick_source = g_timeout_add_seconds_full(
        G_PRIORITY_DEFAULT, 5, refresh_live_metrics, live_ref(live),
        live_unref);
    live->brightness_tick_source = g_timeout_add_seconds_full(
        G_PRIORITY_DEFAULT, 2, refresh_brightness_metric, live_ref(live),
        live_unref);
}

void menu_live_stop(MenuApp *app) {
    if (!app || !app->live) return;
    MenuLive *live = app->live;
    app->live = NULL;
    live->stopping = TRUE;

    if (live->dispatch_source) g_source_remove(live->dispatch_source);
    if (live->page_tick_source) g_source_remove(live->page_tick_source);
    if (live->brightness_tick_source)
        g_source_remove(live->brightness_tick_source);
    if (live->audio_restart_source)
        g_source_remove(live->audio_restart_source);
    if (live->audio_page_source)
        g_source_remove(live->audio_page_source);
    if (live->hypr_restart_source)
        g_source_remove(live->hypr_restart_source);

    if (live->system_bus) {
        if (live->network_subscription)
            g_dbus_connection_signal_unsubscribe(
                live->system_bus, live->network_subscription);
        if (live->network_topology_subscription)
            g_dbus_connection_signal_unsubscribe(
                live->system_bus, live->network_topology_subscription);
        if (live->wireless_topology_subscription)
            g_dbus_connection_signal_unsubscribe(
                live->system_bus, live->wireless_topology_subscription);
        if (live->bluez_subscription)
            g_dbus_connection_signal_unsubscribe(
                live->system_bus, live->bluez_subscription);
        if (live->upower_subscription)
            g_dbus_connection_signal_unsubscribe(
                live->system_bus, live->upower_subscription);
        if (live->power_profiles_subscription)
            g_dbus_connection_signal_unsubscribe(
                live->system_bus, live->power_profiles_subscription);
    }
    if (live->session_bus) {
        if (live->mpris_subscription)
            g_dbus_connection_signal_unsubscribe(
                live->session_bus, live->mpris_subscription);
        if (live->mpris_owner_subscription)
            g_dbus_connection_signal_unsubscribe(
                live->session_bus, live->mpris_owner_subscription);
        if (live->swaync_subscription)
            g_dbus_connection_signal_unsubscribe(
                live->session_bus, live->swaync_subscription);
    }
    if (live->app_info_monitor && live->app_info_handler) {
        g_signal_handler_disconnect(live->app_info_monitor,
                                    live->app_info_handler);
        live->app_info_handler = 0;
    }

    g_cancellable_cancel(live->cancel);
    if (live->calendar_monitor)
        g_file_monitor_cancel(live->calendar_monitor);
    if (live->clipboard_monitor)
        g_file_monitor_cancel(live->clipboard_monitor);
    if (live->audio_process)
        g_subprocess_force_exit(live->audio_process);
    if (live->hypr_connection)
        g_io_stream_close(G_IO_STREAM(live->hypr_connection), NULL, NULL);

    g_clear_object(&live->calendar_monitor);
    g_clear_object(&live->clipboard_monitor);
    g_clear_object(&live->audio_stream);
    g_clear_object(&live->audio_process);
    g_clear_object(&live->hypr_stream);
    g_clear_object(&live->hypr_connection);
    g_clear_object(&live->system_bus);
    g_clear_object(&live->session_bus);
    g_clear_object(&live->app_info_monitor);
    g_clear_object(&live->cancel);
    live_unref(live);
}
