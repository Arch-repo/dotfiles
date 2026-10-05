#include "local_config.h"
#include "widgets.h"
#include <string.h>

static gboolean owner_exists(void) {
  g_autoptr(GDBusConnection) bus =
      g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL);
  if (!bus)
    return FALSE;
  g_autoptr(GVariant) reply = g_dbus_connection_call_sync(
      bus, "org.freedesktop.DBus", "/org/freedesktop/DBus",
      "org.freedesktop.DBus", "NameHasOwner",
      g_variant_new("(s)", WIDGET_APP_ID), G_VARIANT_TYPE("(b)"),
      G_DBUS_CALL_FLAGS_NONE, 1000, NULL, NULL);
  gboolean running = FALSE;
  if (reply)
    g_variant_get(reply, "(b)", &running);
  return running;
}
static gboolean configuration_command(const char *name) {
  return !strcmp(name, "enable") || !strcmp(name, "disable") ||
         !strcmp(name, "lock") || !strcmp(name, "unlock") ||
         !strcmp(name, "toggle-lock") || !strcmp(name, "set-visible") ||
         !strcmp(name, "set-autostart") || !strcmp(name, "reset-layout");
}
static gboolean valid_configuration(const char *name, const char *id,
                                    const char *value) {
  if (!strcmp(name, "set-visible"))
    return widget_kind(id) >= 0 && (!strcmp(value, "0") || !strcmp(value, "1"));
  if (!strcmp(name, "set-autostart"))
    return !strcmp(value, "0") || !strcmp(value, "1");
  return configuration_command(name);
}
static void mutate(WidgetSettings *settings, const char *name, const char *id,
                   const char *value) {
  if (!strcmp(name, "enable") || !strcmp(name, "disable"))
    settings->enabled = !strcmp(name, "enable");
  else if (!strcmp(name, "set-visible"))
    settings->visible[widget_kind(id)] = !strcmp(value, "1");
  else if (!strcmp(name, "set-autostart"))
    settings->autostart = !strcmp(value, "1");
  else if (!strcmp(name, "reset-layout")) {
    json_object_put(settings->layouts);
    settings->layouts = json_object_new_object();
  } else
    settings->locked = !strcmp(name, "lock") ||
                       (!strcmp(name, "toggle-lock") && !settings->locked);
}
static void synchronize(WidgetApp *app, gboolean restart) {
  if (restart || !app->settings.enabled)
    widget_app_stop(app);
  if (app->settings.enabled) {
    if (!app->started)
      widget_app_start(app);
    else {
      widget_app_rebuild(app);
      for (guint i = 0; i < app->windows->len; i++)
        widget_window_sync(g_ptr_array_index(app->windows, i));
    }
  } else
    g_application_quit(G_APPLICATION(app->application));
}
static void save_windows(WidgetApp *app) {
  if (!app->windows)
    return;
  for (guint i = 0; i < app->windows->len; i++) {
    WidgetWindow *w = g_ptr_array_index(app->windows, i);
    widget_geometry_save(&app->settings, w->kind, w->output, w->geometry);
  }
}
static void activate(GtkApplication *application, gpointer data) {
  (void)application;
  widget_app_start(data);
}
static void command(GSimpleAction *action, GVariant *parameter, gpointer data) {
  (void)action;
  WidgetApp *app = data;
  const char *name = g_variant_get_string(parameter, NULL);
  if (!strcmp(name, "stop")) {
    widget_app_stop(app);
    g_application_quit(G_APPLICATION(app->application));
    return;
  }
  if (!strcmp(name, "save-layout")) {
    save_windows(app);
    widget_settings_save(&app->settings, NULL);
    return;
  }
  WidgetSettings next = {0};
  if (!widget_settings_load(&next, NULL)) {
    widget_settings_clear(&next);
    return;
  }
  gboolean restart =
      !strcmp(name, "restart") ||
      memcmp(next.visible, app->settings.visible, sizeof(next.visible));
  widget_settings_clear(&app->settings);
  app->settings = next;
  synchronize(app, restart);
}
static void configure(GSimpleAction *action, GVariant *parameter,
                      gpointer data) {
  (void)action;
  WidgetApp *app = data;
  const char *name, *id, *value;
  g_variant_get(parameter, "(&s&s&s)", &name, &id, &value);
  if (!valid_configuration(name, id, value))
    return;
  /* Running settings and drag geometry have one writer: the GTK main thread. */
  save_windows(app);
  WidgetSettings previous = app->settings;
  app->settings.layouts = json_object_get(previous.layouts);
  mutate(&app->settings, name, id, value);
  g_autoptr(GError) error = NULL;
  if (!widget_settings_save(&app->settings, &error)) {
    widget_settings_clear(&app->settings);
    app->settings = previous;
    g_warning("widgets: %s", error->message);
    return;
  }
  gboolean restart = memcmp(previous.visible, app->settings.visible,
                            sizeof(previous.visible)) != 0;
  widget_settings_clear(&previous);
  synchronize(app, restart);
}
static gboolean remote_action(const char *name, GVariant *parameter) {
  g_autoptr(GApplication) control =
      g_application_new(WIDGET_APP_ID, G_APPLICATION_DEFAULT_FLAGS);
  if (!g_application_register(control, NULL, NULL) ||
      !g_application_get_is_remote(control))
    return FALSE;
  g_action_group_activate_action(G_ACTION_GROUP(control), name, parameter);
  g_autoptr(GDBusConnection) bus =
      g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL);
  if (bus)
    g_dbus_connection_flush_sync(bus, NULL, NULL);
  return TRUE;
}
static gboolean remote_command(const char *name) {
  return remote_action("command", g_variant_new_string(name));
}
int main(int argc, char **argv) {
  const char *cmd = argc > 1 ? argv[1] : "toggle";
  const char *id = argc > 2 ? argv[2] : "";
  const char *value = argc > 3 ? argv[3] : "";
  if (!strcmp(cmd, "set-autostart"))
    value = id;
  if (configuration_command(cmd) &&
      (!valid_configuration(cmd, id, value) ||
       (!strcmp(cmd, "set-visible") && argc != 4) ||
       (!strcmp(cmd, "set-autostart") && argc != 3))) {
    g_printerr("widgets: opzione non valida\n");
    return 2;
  }
  gboolean running = owner_exists();
  if (running && configuration_command(cmd))
    return remote_action("configure", g_variant_new("(sss)", cmd, id, value))
               ? 0
               : 1;

  WidgetApp app = {0};
  g_autoptr(GError) error = NULL;
  /* Offline edits serialize load + mutation, rather than just the final write.
   */
  int lock = anto_local_config_lock("widgets/control.lock", TRUE, &error);
  if (lock < 0 || !widget_settings_load(&app.settings, &error)) {
    g_printerr("widgets: %s\n",
               error ? error->message : "stato non disponibile");
    anto_local_config_unlock(lock);
    widget_settings_clear(&app.settings);
    return 1;
  }
  gboolean start = FALSE;
  int result = 0;
  if (!strcmp(cmd, "status"))
    g_print("%s\n", !app.settings.enabled ? "disabled"
                    : running             ? "running"
                                          : "stopped");
  else if (!strcmp(cmd, "list-widgets")) {
    for (guint i = 0; i < widget_definition_count; i++)
      g_print("%s|%s\n", widget_definitions[i]->id,
              widget_definitions[i]->name);
  } else if (configuration_command(cmd)) {
    /* A process may have acquired ownership while this client waited. */
    if (owner_exists())
      result =
          remote_action("configure", g_variant_new("(sss)", cmd, id, value))
              ? 0
              : 1;
    else {
      mutate(&app.settings, cmd, id, value);
      if (!widget_settings_save(&app.settings, &error))
        result = 1;
    }
  } else if (!strcmp(cmd, "stop") || !strcmp(cmd, "save-layout")) {
    if (running)
      result = remote_command(cmd) ? 0 : 1;
  } else if (!strcmp(cmd, "write-rules")) {
  } /* Native layer windows need no generated Hyprland window rules. */
  else if (!strcmp(cmd, "restart") || !strcmp(cmd, "reload") ||
           !strcmp(cmd, "apply-layout")) {
    if (running)
      result = remote_command(!strcmp(cmd, "restart") ? "restart" : "reload")
                   ? 0
                   : 1;
    else
      start = app.settings.enabled;
  } else if (!strcmp(cmd, "autostart"))
    start = app.settings.enabled && app.settings.autostart;
  else if (!strcmp(cmd, "start") || !strcmp(cmd, "daemon"))
    start = app.settings.enabled;
  else if (!strcmp(cmd, "toggle")) {
    if (running)
      result = remote_command("stop") ? 0 : 1;
    else
      start = app.settings.enabled;
  } else if (!strcmp(cmd, "is-running")) {
    g_print("%s\n", running ? "running" : "stopped");
    result = running ? 0 : 1;
  } else {
    g_printerr("widgets: comando non valido: %s\n", cmd);
    result = 2;
  }
  anto_local_config_unlock(lock);
  if (start && !running) {
    app.application =
        gtk_application_new(WIDGET_APP_ID, G_APPLICATION_DEFAULT_FLAGS);
    GSimpleAction *control =
        g_simple_action_new("command", G_VARIANT_TYPE_STRING);
    g_signal_connect(control, "activate", G_CALLBACK(command), &app);
    g_action_map_add_action(G_ACTION_MAP(app.application), G_ACTION(control));
    g_object_unref(control);
    control = g_simple_action_new("configure", G_VARIANT_TYPE("(sss)"));
    g_signal_connect(control, "activate", G_CALLBACK(configure), &app);
    g_action_map_add_action(G_ACTION_MAP(app.application), G_ACTION(control));
    g_object_unref(control);
    g_signal_connect(app.application, "activate", G_CALLBACK(activate), &app);
    char *args[] = {argv[0], NULL};
    result = g_application_run(G_APPLICATION(app.application), 1, args);
    widget_app_stop(&app);
    g_object_unref(app.application);
  }
  if (error)
    g_printerr("widgets: %s\n", error->message);
  widget_settings_clear(&app.settings);
  return result;
}
