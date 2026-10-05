#pragma once
#include "calendar_data.h"
#include "primitives.h"
#include <gtk/gtk.h>
#include <json-c/json.h>

#define WIDGET_APP_ID "com.anto426.Widgets"
#define WIDGET_LIMIT 64
#define WIDGET_BARS 32
typedef guint WidgetKind;
typedef struct _WidgetStore WidgetStore;
typedef struct {
  const char *id, *name, *icon, *detail, *page, *legacy_ids;
  int width, height;
  gboolean right, bottom;
  int default_y;
  guint features;
  GtkWidget *(*create)(WidgetStore *store);
} WidgetDefinition;
extern const WidgetDefinition *const widget_definitions[];
extern const guint widget_definition_count;
#define WIDGET_DECLARE(identifier, label, symbol, description, menu_page,      \
                       aliases, w, h, align_right, align_bottom, y,            \
                       data_features, factory)                                 \
  const WidgetDefinition widget_definition_##identifier = {                    \
      #identifier, label, symbol,      description,  menu_page, aliases,       \
      w,           h,     align_right, align_bottom, y,         data_features, \
      factory}
typedef struct {
  gboolean enabled, autostart, locked, visible[WIDGET_LIMIT];
  json_object *layouts;
} WidgetSettings;
typedef struct {
  int x, y, width, height;
} WidgetGeometry;
gboolean widget_settings_load(WidgetSettings *settings, GError **error);
gboolean widget_settings_save(WidgetSettings *settings, GError **error);
void widget_settings_clear(WidgetSettings *settings);
WidgetGeometry widget_geometry(WidgetSettings *settings, WidgetKind kind,
                               const char *output, int width, int height);
void widget_geometry_save(WidgetSettings *settings, WidgetKind kind,
                          const char *output, WidgetGeometry geometry);
int widget_kind(const char *id);

#define WIDGET_TYPE_STORE (widget_store_get_type())
G_DECLARE_FINAL_TYPE(WidgetStore, widget_store, WIDGET, STORE, GObject)
enum {
  W_CHANGED_CLOCK = 1,
  W_CHANGED_HARDWARE = 2,
  W_CHANGED_AGENDA = 4,
  W_CHANGED_MEDIA = 8,
  W_CHANGED_SPECTRUM = 16,
  W_CHANGED_ALL = 31
};
struct _WidgetStore {
  GObject parent;
  GCancellable *cancel;
  gboolean stopped, agenda_busy, agenda_pending, media_busy, media_pending;
  guint clock_timer, hardware_timer, agenda_debounce;
  GFileMonitor *agenda_monitor;
  GPtrArray *events;
  guint64 cpu_total, cpu_idle;
  double cpu, memory, disk, battery;
  char *machine, *platform, *memory_text, *battery_text;
  char *player, *track, *artist, *art;
  gboolean playing, remote_media, can_play, can_pause, can_next, can_previous;
  GDBusConnection *bus;
  guint media_subscription, owner_subscription;
  GSubprocess *spectrum_process;
  GDataInputStream *spectrum_stream;
  double bars[WIDGET_BARS];
  gboolean audio_available;
};
WidgetStore *widget_store_new(void);
void widget_store_start(WidgetStore *store,
                        const gboolean visible[WIDGET_LIMIT]);
void widget_store_stop(WidgetStore *store);
void widget_store_changed(WidgetStore *store, guint fields);
void widget_hardware_update(WidgetStore *store);
void widget_agenda_start(WidgetStore *store);
void widget_agenda_request(WidgetStore *store);
void widget_media_start(WidgetStore *store);
void widget_media_refresh(WidgetStore *store);
void widget_media_control(WidgetStore *store, const char *method);
void widget_spectrum_start(WidgetStore *store);
void widget_spectrum_parse(WidgetStore *store, const char *line);

typedef struct _WidgetApp WidgetApp;
typedef struct {
  WidgetApp *app;
  GtkWindow *window;
  GdkMonitor *monitor;
  WidgetKind kind;
  char *output;
  WidgetGeometry geometry;
  int drag_x, drag_y;
  gulong geometry_handler;
} WidgetWindow;
struct _WidgetApp {
  GtkApplication *application;
  WidgetSettings settings;
  WidgetStore *store;
  GPtrArray *windows;
  GListModel *monitors;
  gulong monitors_handler;
  gboolean started;
};
void widget_app_start(WidgetApp *app);
void widget_app_stop(WidgetApp *app);
void widget_app_rebuild(WidgetApp *app);
void widget_window_free(gpointer data);
void widget_window_sync(WidgetWindow *window);
WidgetWindow *widget_window_new(WidgetApp *app, WidgetKind kind,
                                GdkMonitor *monitor, const char *output);
GtkWidget *widget_view_new(WidgetStore *store, WidgetKind kind,
                           GtkWidget **header);
GtkWidget *widget_clock_new(WidgetStore *store);
GtkWidget *widget_system_new(WidgetStore *store);
GtkWidget *widget_agenda_new(WidgetStore *store);
GtkWidget *widget_spectrum_new(WidgetStore *store);
GtkWidget *widget_media_new(WidgetStore *store);
void widget_open_menu(const char *page);
void widget_label(GtkWidget *label, const char *text);
