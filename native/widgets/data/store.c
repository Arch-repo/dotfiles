#include "widgets.h"
G_DEFINE_TYPE(WidgetStore, widget_store, G_TYPE_OBJECT)
static guint changed_signal;
void widget_store_changed(WidgetStore *s, guint fields) {
  if (!s->stopped)
    g_signal_emit(s, changed_signal, 0, fields);
}
static void finalize(GObject *object) {
  WidgetStore *s = WIDGET_STORE(object);
  widget_store_stop(s);
  g_clear_object(&s->cancel);
  g_clear_pointer(&s->events, g_ptr_array_unref);
  g_free(s->machine);
  g_free(s->platform);
  g_free(s->memory_text);
  g_free(s->battery_text);
  g_free(s->player);
  g_free(s->track);
  g_free(s->artist);
  g_free(s->art);
  G_OBJECT_CLASS(widget_store_parent_class)->finalize(object);
}
static void widget_store_class_init(WidgetStoreClass *class) {
  G_OBJECT_CLASS(class)->finalize = finalize;
  changed_signal =
      g_signal_new("changed", G_TYPE_FROM_CLASS(class), G_SIGNAL_RUN_LAST, 0,
                   NULL, NULL, NULL, G_TYPE_NONE, 1, G_TYPE_UINT);
}
static void widget_store_init(WidgetStore *s) {
  s->cancel = g_cancellable_new();
  s->events = g_ptr_array_new_with_free_func(anto_calendar_event_free);
  s->battery = -1;
  s->machine = g_strdup(g_get_host_name());
  s->platform = g_strdup("Linux");
  s->memory_text = g_strdup("—");
  s->battery_text = g_strdup("Non rilevata");
  s->track = g_strdup("Nessuna riproduzione");
  s->artist = g_strdup("Apri la tua app musicale");
}
WidgetStore *widget_store_new(void) {
  return g_object_new(WIDGET_TYPE_STORE, NULL);
}
static gboolean clock_tick(gpointer data) {
  widget_store_changed(data, W_CHANGED_CLOCK);
  return G_SOURCE_CONTINUE;
}
static gboolean hardware_tick(gpointer data) {
  widget_hardware_update(data);
  return G_SOURCE_CONTINUE;
}
void widget_store_start(WidgetStore *s, const gboolean visible[WIDGET_LIMIT]) {
  guint features = 0;
  for (guint i = 0; i < widget_definition_count; i++)
    if (visible[i])
      features |= widget_definitions[i]->features;
  if (features & W_CHANGED_CLOCK)
    s->clock_timer = g_timeout_add_seconds(1, clock_tick, s);
  if (features & W_CHANGED_HARDWARE) {
    widget_hardware_update(s);
    s->hardware_timer = g_timeout_add_seconds(2, hardware_tick, s);
  }
  if (features & W_CHANGED_AGENDA)
    widget_agenda_start(s);
  if (features & W_CHANGED_MEDIA)
    widget_media_start(s);
  if (features & W_CHANGED_SPECTRUM)
    widget_spectrum_start(s);
}
void widget_store_stop(WidgetStore *s) {
  if (s->stopped)
    return;
  s->stopped = TRUE;
  g_cancellable_cancel(s->cancel);
  guint *timers[] = {&s->clock_timer, &s->hardware_timer, &s->agenda_debounce};
  for (guint i = 0; i < G_N_ELEMENTS(timers); i++)
    if (*timers[i]) {
      g_source_remove(*timers[i]);
      *timers[i] = 0;
    }
  if (s->agenda_monitor)
    g_file_monitor_cancel(s->agenda_monitor);
  g_clear_object(&s->agenda_monitor);
  if (s->bus && s->media_subscription)
    g_dbus_connection_signal_unsubscribe(s->bus, s->media_subscription);
  if (s->bus && s->owner_subscription)
    g_dbus_connection_signal_unsubscribe(s->bus, s->owner_subscription);
  g_clear_object(&s->bus);
  if (s->spectrum_process)
    g_subprocess_force_exit(s->spectrum_process);
  g_clear_object(&s->spectrum_process);
  g_clear_object(&s->spectrum_stream);
}
