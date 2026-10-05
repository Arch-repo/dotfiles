#pragma once
#include <gio/gio.h>
#include <json-c/json.h>
typedef struct {
  char *id;
  char *source;
  char *date;
  char *title;
  char *start;
  char *end;
  char *description;
  gboolean all_day;
} CalendarEvent;
extern const char *const anto_calendar_italian_days[8];
extern const char *const anto_calendar_italian_months[13];
char *anto_calendar_italian_date(GDateTime *date, gboolean include_time);
const char *anto_calendar_json_string(struct json_object *object,
                                      const char *key);
void anto_calendar_event_free(gpointer data);
int anto_calendar_compare_events(gconstpointer left, gconstpointer right);
void anto_calendar_load_events_file(GPtrArray *events, GHashTable *seen,
                                    const char *path);
GPtrArray *anto_calendar_read_events(void);
