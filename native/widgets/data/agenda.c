#include "widgets.h"
#include <glib/gstdio.h>
static void read_events(GTask *, gpointer, gpointer, GCancellable *);
static void loaded(GObject *object, GAsyncResult *result, gpointer data) {
  (void)data;
  WidgetStore *s = WIDGET_STORE(object);
  s->agenda_busy = FALSE;
  g_autoptr(GError) error = NULL;
  GPtrArray *events = g_task_propagate_pointer(G_TASK(result), &error);
  if (events && !s->stopped) {
    g_ptr_array_unref(s->events);
    s->events = events;
    widget_store_changed(s, W_CHANGED_AGENDA);
  } else if (events)
    g_ptr_array_unref(events);
  if (s->agenda_pending && !s->stopped) {
    s->agenda_pending = FALSE;
    widget_agenda_request(s);
  }
}
void widget_agenda_request(WidgetStore *s) {
  if (s->stopped)
    return;
  if (s->agenda_busy) {
    s->agenda_pending = TRUE;
    return;
  }
  s->agenda_busy = TRUE;
  GTask *task = g_task_new(s, s->cancel, loaded, NULL);
  g_task_run_in_thread(task, read_events);
  g_object_unref(task);
}
static void read_events(GTask *task, gpointer source, gpointer data,
                        GCancellable *cancel) {
  (void)source;
  (void)data;
  (void)cancel;
  if (!g_task_return_error_if_cancelled(task))
    g_task_return_pointer(task, anto_calendar_read_events(),
                          (GDestroyNotify)g_ptr_array_unref);
}
static gboolean reload(gpointer data) {
  WidgetStore *s = data;
  s->agenda_debounce = 0;
  widget_agenda_request(s);
  return G_SOURCE_REMOVE;
}
static void files_changed(GFileMonitor *monitor, GFile *file, GFile *other,
                          GFileMonitorEvent event, gpointer data) {
  (void)monitor;
  (void)file;
  (void)other;
  (void)event;
  WidgetStore *s = data;
  if (!s->stopped && !s->agenda_debounce)
    s->agenda_debounce = g_timeout_add(120, reload, s);
}
void widget_agenda_start(WidgetStore *s) {
  g_autofree char *path =
      g_build_filename(g_get_user_data_dir(), "anto426", "calendar", NULL);
  g_mkdir_with_parents(path, 0700);
  g_autoptr(GFile) file = g_file_new_for_path(path);
  s->agenda_monitor = g_file_monitor_directory(file, G_FILE_MONITOR_WATCH_MOVES,
                                               s->cancel, NULL);
  if (s->agenda_monitor)
    g_signal_connect(s->agenda_monitor, "changed", G_CALLBACK(files_changed),
                     s);
  widget_agenda_request(s);
}
