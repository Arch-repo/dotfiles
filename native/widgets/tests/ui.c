#include "widgets.h"
#include <glib/gstdio.h>
#include <gtk4-layer-shell.h>
static guint calls;
static gboolean playing = TRUE;
static void settle(int ms) {
  gint64 until = g_get_monotonic_time() + ms * 1000;
  while (g_get_monotonic_time() < until) {
    while (g_main_context_iteration(NULL, FALSE))
      ;
    g_usleep(1000);
  }
}
static GVariant *property(GDBusConnection *bus, const char *sender,
                          const char *path, const char *iface, const char *name,
                          GError **error, gpointer data) {
  (void)bus;
  (void)sender;
  (void)path;
  (void)iface;
  (void)error;
  (void)data;
  if (!strcmp(name, "PlaybackStatus"))
    return g_variant_new_string(playing ? "Playing" : "Paused");
  if (!strcmp(name, "Metadata")) {
    GVariantBuilder b;
    g_variant_builder_init(&b, G_VARIANT_TYPE_VARDICT);
    g_variant_builder_add(&b, "{sv}", "xesam:title",
                          g_variant_new_string("La tua musica"));
    const char *artists[] = {"Player di prova", NULL};
    g_variant_builder_add(&b, "{sv}", "xesam:artist",
                          g_variant_new_strv(artists, -1));
    return g_variant_builder_end(&b);
  }
  return g_variant_new_boolean(TRUE);
}
static void method(GDBusConnection *bus, const char *sender, const char *path,
                   const char *iface, const char *name, GVariant *args,
                   GDBusMethodInvocation *invocation, gpointer data) {
  (void)sender;
  (void)iface;
  (void)args;
  (void)data;
  calls++;
  if (!strcmp(name, "PlayPause"))
    playing = !playing;
  GVariantBuilder changed;
  g_variant_builder_init(&changed, G_VARIANT_TYPE_VARDICT);
  g_variant_builder_add(&changed, "{sv}", "PlaybackStatus",
                        g_variant_new_string(playing ? "Playing" : "Paused"));
  g_dbus_connection_emit_signal(
      bus, NULL, path, "org.freedesktop.DBus.Properties", "PropertiesChanged",
      g_variant_new("(sa{sv}as)", "org.mpris.MediaPlayer2.Player", &changed,
                    NULL),
      NULL);
  g_dbus_method_invocation_return_value(invocation, NULL);
}
static GtkWidget *find_button(GtkWidget *root, const char *tooltip) {
  if (GTK_IS_BUTTON(root) &&
      !g_strcmp0(gtk_widget_get_tooltip_text(root), tooltip))
    return root;
  for (GtkWidget *child = gtk_widget_get_first_child(root); child;
       child = gtk_widget_get_next_sibling(child)) {
    GtkWidget *found = find_button(child, tooltip);
    if (found)
      return found;
  }
  return NULL;
}
static void capture(void) {
  const char *path = g_getenv("ANTO_WIDGET_CAPTURE");
  if (!path)
    return;
  g_autoptr(GSubprocess) child =
      g_subprocess_new(G_SUBPROCESS_FLAGS_NONE, NULL, "grim", "-o",
                       g_getenv("ANTO426_TARGET_MONITOR"), path, NULL);
  g_assert_true(g_subprocess_wait_check(child, NULL, NULL));
}
int main(void) {
  if (!gtk_init_check())
    return 77;
  g_autoptr(GDBusConnection) bus =
      g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL);
  g_assert_nonnull(bus);
  const char *xml =
      "<node><interface name='org.mpris.MediaPlayer2.Player'><method "
      "name='PlayPause'/><method name='Next'/><method "
      "name='Previous'/><property name='PlaybackStatus' type='s' "
      "access='read'/><property name='Metadata' type='a{sv}' "
      "access='read'/><property name='CanControl' type='b' "
      "access='read'/><property name='CanPlay' type='b' "
      "access='read'/><property name='CanPause' type='b' "
      "access='read'/><property name='CanGoNext' type='b' "
      "access='read'/><property name='CanGoPrevious' type='b' "
      "access='read'/></interface></node>";
  g_autoptr(GDBusNodeInfo) info = g_dbus_node_info_new_for_xml(xml, NULL);
  GDBusInterfaceVTable vtable = {.method_call = method,
                                 .get_property = property};
  guint registration = g_dbus_connection_register_object(
      bus, "/org/mpris/MediaPlayer2", info->interfaces[0], &vtable, NULL, NULL,
      NULL);
  g_assert_cmpuint(registration, >, 0);
  g_autoptr(GVariant) owner = g_dbus_connection_call_sync(
      bus, "org.freedesktop.DBus", "/org/freedesktop/DBus",
      "org.freedesktop.DBus", "RequestName",
      g_variant_new("(su)", "org.mpris.MediaPlayer2.AntoWidgetFixture", 0),
      NULL, G_DBUS_CALL_FLAGS_NONE, 1000, NULL, NULL);
  g_assert_nonnull(owner);
  g_autofree char *directory =
      g_build_filename(g_get_user_data_dir(), "anto426/calendar", NULL);
  g_mkdir_with_parents(directory, 0700);
  g_autofree char *events = g_build_filename(directory, "events.json", NULL);
  g_autoptr(GDateTime) now = g_date_time_new_now_local();
  g_autofree char *date = g_date_time_format(now, "%Y-%m-%d");
  g_autofree char *json = g_strdup_printf(
      "[{\"date\":\"%s\",\"title\":\"Un giorno tutto tuo\",\"all_day\":true}]",
      date);
  g_file_set_contents(events, json, -1, NULL);
  WidgetApp app = {0};
  g_assert_true(widget_settings_load(&app.settings, NULL));
  app.settings.enabled = TRUE;
  for (guint i = 0; i < widget_definition_count; i++)
    app.settings.visible[i] = TRUE;
  app.application = gtk_application_new("com.anto426.WidgetFixture",
                                        G_APPLICATION_NON_UNIQUE);
  g_assert_true(
      g_application_register(G_APPLICATION(app.application), NULL, NULL));
  widget_app_start(&app);
  settle(900);
  g_assert_cmpuint(app.windows->len, ==, widget_definition_count);
  g_assert_cmpuint(app.store->events->len, ==, 1);
  g_assert_cmpstr(app.store->track, ==, "La tua musica");
  g_assert_true(app.store->can_play);
  if (g_getenv("ANTO_WIDGET_TEST_AUDIO")) {
    g_assert_true(app.store->audio_available);
    g_assert_cmpfloat(app.store->bars[0], >, 0);
    g_assert_cmpfloat(app.store->bars[WIDGET_BARS - 1], >, 0);
  }
  WidgetWindow *media = NULL;
  for (guint i = 0; i < app.windows->len; i++) {
    WidgetWindow *w = g_ptr_array_index(app.windows, i);
    if (!strcmp(widget_definitions[w->kind]->id, "media"))
      media = w;
    if (gtk_layer_is_layer_window(w->window)) {
      g_assert_cmpint(gtk_layer_get_layer(w->window), ==,
                      GTK_LAYER_SHELL_LAYER_BOTTOM);
      g_assert_cmpint(gtk_layer_get_keyboard_mode(w->window), ==,
                      GTK_LAYER_SHELL_KEYBOARD_MODE_NONE);
    }
  }
  g_assert_nonnull(media);
  GtkWidget *play = find_button(GTK_WIDGET(media->window), "Play / pausa");
  g_assert_nonnull(play);
  g_assert_true(gtk_widget_get_sensitive(play));
  const char *ready = g_getenv("ANTO_WIDGET_READY");
  GtkWindow *focus_probe = NULL;
  if (ready) {
    focus_probe = GTK_WINDOW(gtk_application_window_new(app.application));
    gtk_window_set_title(focus_probe, "Widget focus probe");
    gtk_window_set_default_size(focus_probe, 420, 100);
    GtkWidget *entry = NULL;
    GtkWidget *field = anto_ui_field("Il focus resta qui",
                                     "Scrivi nella tua applicazione", &entry);
    gtk_window_set_child(focus_probe, field);
    gtk_window_present(focus_probe);
    gtk_widget_grab_focus(entry);
    settle(250);

    graphene_rect_t bounds;
    g_assert_true(
        gtk_widget_compute_bounds(play, GTK_WIDGET(media->window), &bounds));
    g_autofree char *point = g_strdup_printf(
        "%d %d",
        media->geometry.x + (int)(bounds.origin.x + bounds.size.width / 2),
        media->geometry.y + (int)(bounds.origin.y + bounds.size.height / 2));
    g_file_set_contents(ready, point, -1, NULL);
    capture();
    for (int i = 0; i < 300 && !calls; i++)
      settle(20);
  } else
    g_signal_emit_by_name(play, "clicked");
  settle(200);
  g_assert_cmpuint(calls, ==, 1);
  g_assert_false(app.store->playing);
  g_assert_true(app.settings.locked);
  g_assert_true(gtk_widget_get_sensitive(play));
  GtkWidget *next =
      find_button(GTK_WIDGET(media->window), "Traccia successiva");
  g_signal_emit_by_name(next, "clicked");
  settle(150);
  g_assert_cmpuint(calls, ==, 2);
  g_file_set_contents(events, "[]", -1, NULL);
  settle(300);
  g_assert_cmpuint(app.store->events->len, ==, 0);
  /* Prefer a concrete local player over its proxy and detect remote control. */
  g_autoptr(GVariant) remote = g_dbus_connection_call_sync(
      bus, "org.freedesktop.DBus", "/org/freedesktop/DBus",
      "org.freedesktop.DBus", "RequestName",
      g_variant_new("(su)", "org.mpris.MediaPlayer2.kdeconnect.fixture", 0),
      NULL, G_DBUS_CALL_FLAGS_NONE, 1000, NULL, NULL);
  g_assert_nonnull(remote);
  settle(150);
  g_assert_false(app.store->remote_media);
  g_autoptr(GVariant) release = g_dbus_connection_call_sync(
      bus, "org.freedesktop.DBus", "/org/freedesktop/DBus",
      "org.freedesktop.DBus", "ReleaseName",
      g_variant_new("(s)", "org.mpris.MediaPlayer2.AntoWidgetFixture"), NULL,
      G_DBUS_CALL_FLAGS_NONE, 1000, NULL, NULL);
  g_assert_nonnull(release);
  settle(150);
  g_assert_true(app.store->remote_media);
  guint before = app.windows->len;
  widget_app_rebuild(&app);
  g_assert_cmpuint(app.windows->len, ==, before);
  app.settings.locked = FALSE;
  for (guint i = 0; i < app.windows->len; i++)
    widget_window_sync(g_ptr_array_index(app.windows, i));
  g_assert_true(gtk_widget_get_sensitive(play));
  if (focus_probe)
    gtk_window_destroy(focus_probe);
  widget_store_stop(app.store);
  widget_app_stop(&app);
  widget_settings_clear(&app.settings);
  g_object_unref(app.application);
  settle(150);
  g_dbus_connection_unregister_object(bus, registration);
  g_print("native widgets: registry, shared views and data, agenda updates, "
          "MPRIS play/next, geometry-only lock, keyboard NONE, idempotent "
          "reconciliation and task disposal verified\n");
  return 0;
}
