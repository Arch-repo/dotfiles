#include "widgets.h"
#include <string.h>
#define PLAYER_PATH "/org/mpris/MediaPlayer2"
#define PLAYER_INTERFACE "org.mpris.MediaPlayer2.Player"
typedef struct {
  WidgetStore *store;
  guint pending;
  int score;
  char *name, *track, *artist, *art;
  gboolean playing, play, pause, next, previous;
} Scan;
typedef struct {
  Scan *scan;
  char *name;
} Request;
static gboolean flag(GVariant *dict, const char *key) {
  gboolean value = FALSE;
  g_variant_lookup(dict, key, "b", &value);
  return value;
}
static void scan_done(Scan *scan) {
  WidgetStore *s = scan->store;
  s->media_busy = FALSE;
  if (!s->stopped) {
    g_free(s->player);
    s->player = g_steal_pointer(&scan->name);
    g_free(s->track);
    s->track = scan->track ? g_steal_pointer(&scan->track)
                           : g_strdup("Nessuna riproduzione");
    g_free(s->artist);
    s->artist = scan->artist ? g_steal_pointer(&scan->artist)
                             : g_strdup("Apri la tua app musicale");
    g_free(s->art);
    s->art = g_steal_pointer(&scan->art);
    s->playing = scan->playing;
    s->remote_media =
        s->player &&
        g_str_has_prefix(s->player, "org.mpris.MediaPlayer2.kdeconnect.");
    s->can_play = scan->play;
    s->can_pause = scan->pause;
    s->can_next = scan->next;
    s->can_previous = scan->previous;
    widget_store_changed(s, W_CHANGED_MEDIA);
    if (s->media_pending) {
      s->media_pending = FALSE;
      widget_media_refresh(s);
    }
  }
  g_free(scan->name);
  g_free(scan->track);
  g_free(scan->artist);
  g_free(scan->art);
  g_object_unref(s);
  g_free(scan);
}
static void properties_ready(GObject *object, GAsyncResult *result,
                             gpointer data) {
  Request *r = data;
  Scan *scan = r->scan;
  WidgetStore *s = scan->store;
  g_autoptr(GError) error = NULL;
  g_autoptr(GVariant) reply =
      g_dbus_connection_call_finish(G_DBUS_CONNECTION(object), result, &error);
  if (reply && !s->stopped) {
    g_autoptr(GVariant) properties = g_variant_get_child_value(reply, 0);
    const char *status = "Stopped";
    g_variant_lookup(properties, "PlaybackStatus", "&s", &status);
    gboolean playing = !g_strcmp0(status, "Playing");
    int score =
        (playing ? 100 : 10) +
        (!g_str_has_prefix(r->name, "org.mpris.MediaPlayer2.kdeconnect.") ? 2
                                                                          : 0) +
        (g_strcmp0(s->player, r->name) == 0 ? 1 : 0);
    if (score > scan->score) {
      scan->score = score;
      g_free(scan->name);
      scan->name = g_strdup(r->name);
      scan->playing = playing;
      gboolean control = flag(properties, "CanControl");
      scan->play = control && flag(properties, "CanPlay");
      scan->pause = control && flag(properties, "CanPause");
      scan->next = control && flag(properties, "CanGoNext");
      scan->previous = control && flag(properties, "CanGoPrevious");
      g_free(scan->track);
      scan->track = NULL;
      g_free(scan->artist);
      scan->artist = NULL;
      g_free(scan->art);
      scan->art = NULL;
      g_autoptr(GVariant) metadata = g_variant_lookup_value(
          properties, "Metadata", G_VARIANT_TYPE_VARDICT);
      if (metadata) {
        const char *title = NULL, *art = NULL;
        g_variant_lookup(metadata, "xesam:title", "&s", &title);
        g_variant_lookup(metadata, "mpris:artUrl", "&s", &art);
        scan->track = g_strdup(title && *title ? title : "La tua musica");
        g_auto(GStrv) artists = NULL;
        g_variant_lookup(metadata, "xesam:artist", "^as", &artists);
        scan->artist =
            artists && artists[0]
                ? g_strjoinv(", ", artists)
                : g_strdup(r->name + strlen("org.mpris.MediaPlayer2."));
        if (art && g_str_has_prefix(art, "file://"))
          scan->art = g_filename_from_uri(art, NULL, NULL);
      }
    }
  }
  g_free(r->name);
  g_free(r);
  if (--scan->pending == 0)
    scan_done(scan);
}
static void names_ready(GObject *object, GAsyncResult *result, gpointer data) {
  Scan *scan = data;
  WidgetStore *s = scan->store;
  g_autoptr(GError) error = NULL;
  g_autoptr(GVariant) reply =
      g_dbus_connection_call_finish(G_DBUS_CONNECTION(object), result, &error);
  if (reply && !s->stopped) {
    g_auto(GStrv) names = NULL;
    g_variant_get(reply, "(^as)", &names);
    gboolean concrete = FALSE;
    for (guint i = 0; names[i]; i++)
      if (g_str_has_prefix(names[i], "org.mpris.MediaPlayer2.") &&
          strcmp(names[i], "org.mpris.MediaPlayer2.playerctld"))
        concrete = TRUE;
    for (guint i = 0; names[i]; i++)
      if (g_str_has_prefix(names[i], "org.mpris.MediaPlayer2.") &&
          !(concrete &&
            !strcmp(names[i], "org.mpris.MediaPlayer2.playerctld")) &&
          scan->pending < 32) {
        Request *r = g_new0(Request, 1);
        r->scan = scan;
        r->name = g_strdup(names[i]);
        scan->pending++;
        g_dbus_connection_call(
            s->bus, r->name, PLAYER_PATH, "org.freedesktop.DBus.Properties",
            "GetAll", g_variant_new("(s)", PLAYER_INTERFACE),
            G_VARIANT_TYPE("(a{sv})"), G_DBUS_CALL_FLAGS_NONE, 1500, s->cancel,
            properties_ready, r);
      }
  }
  if (!scan->pending)
    scan_done(scan);
}
void widget_media_refresh(WidgetStore *s) {
  if (s->stopped || !s->bus)
    return;
  if (s->media_busy) {
    s->media_pending = TRUE;
    return;
  }
  s->media_busy = TRUE;
  Scan *scan = g_new0(Scan, 1);
  scan->store = g_object_ref(s);
  g_dbus_connection_call(
      s->bus, "org.freedesktop.DBus", "/org/freedesktop/DBus",
      "org.freedesktop.DBus", "ListNames", NULL, G_VARIANT_TYPE("(as)"),
      G_DBUS_CALL_FLAGS_NONE, 1500, s->cancel, names_ready, scan);
}
static void media_signal(GDBusConnection *bus, const char *sender,
                         const char *path, const char *interface,
                         const char *signal, GVariant *parameters,
                         gpointer data) {
  (void)bus;
  (void)sender;
  (void)path;
  (void)interface;
  (void)parameters;
  if (!g_strcmp0(signal, "NameOwnerChanged")) {
    const char *name, *old, *next;
    g_variant_get(parameters, "(&s&s&s)", &name, &old, &next);
    if (!g_str_has_prefix(name, "org.mpris.MediaPlayer2."))
      return;
  }
  widget_media_refresh(data);
}
static void bus_ready(GObject *object, GAsyncResult *result, gpointer data) {
  (void)object;
  WidgetStore *s = data;
  g_autoptr(GError) error = NULL;
  GDBusConnection *bus = g_bus_get_finish(result, &error);
  if (bus && !s->stopped) {
    s->bus = bus;
    s->media_subscription = g_dbus_connection_signal_subscribe(
        bus, NULL, "org.freedesktop.DBus.Properties", "PropertiesChanged",
        PLAYER_PATH, NULL, G_DBUS_SIGNAL_FLAGS_NONE, media_signal, s, NULL);
    s->owner_subscription = g_dbus_connection_signal_subscribe(
        bus, "org.freedesktop.DBus", "org.freedesktop.DBus", "NameOwnerChanged",
        "/org/freedesktop/DBus", NULL, G_DBUS_SIGNAL_FLAGS_NONE, media_signal,
        s, NULL);
    widget_media_refresh(s);
  } else if (bus)
    g_object_unref(bus);
  g_object_unref(s);
}
void widget_media_start(WidgetStore *s) {
  g_bus_get(G_BUS_TYPE_SESSION, s->cancel, bus_ready, g_object_ref(s));
}
static void control_done(GObject *object, GAsyncResult *result, gpointer data) {
  WidgetStore *s = data;
  g_autoptr(GError) error = NULL;
  g_autoptr(GVariant) reply =
      g_dbus_connection_call_finish(G_DBUS_CONNECTION(object), result, &error);
  if (!s->stopped)
    widget_media_refresh(s);
  g_object_unref(s);
}
void widget_media_control(WidgetStore *s, const char *method) {
  if (!s->bus || !s->player || s->stopped)
    return;
  gboolean allowed = (!g_strcmp0(method, "Next") && s->can_next) ||
                     (!g_strcmp0(method, "Previous") && s->can_previous) ||
                     (!g_strcmp0(method, "PlayPause") &&
                      (s->playing ? s->can_pause : s->can_play));
  if (allowed)
    g_dbus_connection_call(s->bus, s->player, PLAYER_PATH, PLAYER_INTERFACE,
                           method, NULL, NULL, G_DBUS_CALL_FLAGS_NONE, 1500,
                           s->cancel, control_done, g_object_ref(s));
}
