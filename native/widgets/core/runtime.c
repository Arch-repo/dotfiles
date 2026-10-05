#include "glass.h"
#include "widgets.h"
#include <string.h>
static void monitors_changed(GListModel *model, guint position, guint removed,
                             guint added, gpointer data) {
  (void)model;
  (void)position;
  (void)removed;
  (void)added;
  widget_app_rebuild(data);
}
void widget_app_rebuild(WidgetApp *app) {
  for (guint i = app->windows->len; i > 0; i--) {
    WidgetWindow *w = g_ptr_array_index(app->windows, i - 1);
    if (!gdk_monitor_is_valid(w->monitor) || !app->settings.visible[w->kind])
      g_ptr_array_remove_index(app->windows, i - 1);
  }
  for (guint i = 0; i < g_list_model_get_n_items(app->monitors) && i < 16;
       i++) {
    g_autoptr(GdkMonitor) monitor = g_list_model_get_item(app->monitors, i);
    const char *output = gdk_monitor_get_connector(monitor);
    if (!output)
      output = "default";
    if (g_str_has_prefix(output, "HEADLESS-"))
      continue;
    for (guint kind = 0; kind < widget_definition_count; kind++) {
      if (!app->settings.visible[kind])
        continue;
      gboolean exists = FALSE;
      for (guint j = 0; j < app->windows->len; j++) {
        WidgetWindow *w = g_ptr_array_index(app->windows, j);
        if (w->monitor == monitor && w->kind == (WidgetKind)kind)
          exists = TRUE;
      }
      if (!exists)
        g_ptr_array_add(app->windows,
                        widget_window_new(app, kind, monitor, output));
    }
  }
}
void widget_app_start(WidgetApp *app) {
  if (app->started)
    return;
  app->started = TRUE;
  g_application_hold(G_APPLICATION(app->application));
  anto_ui_init(gdk_display_get_default());
  anto_load_glass_style(gdk_display_get_default());
  g_autofree char *path = g_build_filename(
      g_get_home_dir(), ".local/share/anto-desktop/widgets.css", NULL);
  GdkDisplay *display = gdk_display_get_default();
  if (g_file_test(path, G_FILE_TEST_EXISTS) &&
      !g_object_get_data(G_OBJECT(display), "anto-widget-style")) {
    GtkCssProvider *css = gtk_css_provider_new();
    gtk_css_provider_load_from_path(css, path);
    gtk_style_context_add_provider_for_display(
        display, GTK_STYLE_PROVIDER(css), GTK_STYLE_PROVIDER_PRIORITY_USER + 3);
    g_object_unref(css);
    g_object_set_data(G_OBJECT(display), "anto-widget-style",
                      GINT_TO_POINTER(1));
  }
  app->store = widget_store_new();
  app->windows = g_ptr_array_new_with_free_func(widget_window_free);
  app->monitors =
      g_object_ref(gdk_display_get_monitors(gdk_display_get_default()));
  app->monitors_handler = g_signal_connect(app->monitors, "items-changed",
                                           G_CALLBACK(monitors_changed), app);
  widget_app_rebuild(app);
  widget_store_start(app->store, app->settings.visible);
}
void widget_app_stop(WidgetApp *app) {
  if (!app->started)
    return;
  app->started = FALSE;
  if (app->monitors_handler)
    g_signal_handler_disconnect(app->monitors, app->monitors_handler);
  app->monitors_handler = 0;
  widget_store_stop(app->store);
  g_clear_pointer(&app->windows, g_ptr_array_unref);
  g_clear_object(&app->store);
  g_clear_object(&app->monitors);
  g_application_release(G_APPLICATION(app->application));
}
