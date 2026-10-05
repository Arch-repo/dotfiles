#include "calendar_data.h"
#include <string.h>
const char *const anto_calendar_italian_days[] = {
    "",        "Lunedì",  "Martedì", "Mercoledì",
    "Giovedì", "Venerdì", "Sabato",  "Domenica",
};
const char *const anto_calendar_italian_months[] = {
    "",        "Gennaio",  "Febbraio", "Marzo",  "Aprile",
    "Maggio",  "Giugno",   "Luglio",   "Agosto", "Settembre",
    "Ottobre", "Novembre", "Dicembre",
};

char *anto_calendar_italian_date(GDateTime *date, gboolean include_time) {
  if (!date)
    return g_strdup("");
  const char *day =
      anto_calendar_italian_days[g_date_time_get_day_of_week(date)];
  const char *month = anto_calendar_italian_months[g_date_time_get_month(date)];
  if (include_time)
    return g_strdup_printf(
        "%s %02d %s · %02d:%02d", day, g_date_time_get_day_of_month(date),
        month, g_date_time_get_hour(date), g_date_time_get_minute(date));
  return g_strdup_printf("%s %02d %s %d", day,
                         g_date_time_get_day_of_month(date), month,
                         g_date_time_get_year(date));
}

const char *anto_calendar_json_string(struct json_object *object,
                                      const char *key) {
  struct json_object *value = NULL;
  if (!object || !json_object_is_type(object, json_type_object) ||
      !json_object_object_get_ex(object, key, &value) || !value)
    return "";
  const char *text = json_object_get_string(value);
  return text ? text : "";
}

void anto_calendar_event_free(gpointer data) {
  CalendarEvent *event = data;
  if (!event)
    return;
  g_free(event->id);
  g_free(event->source);
  g_free(event->date);
  g_free(event->title);
  g_free(event->start);
  g_free(event->end);
  g_free(event->description);
  g_free(event);
}

int anto_calendar_compare_events(gconstpointer left, gconstpointer right) {
  const CalendarEvent *a = *(CalendarEvent *const *)left;
  const CalendarEvent *b = *(CalendarEvent *const *)right;
  int date = g_strcmp0(a->date, b->date);
  if (date)
    return date;
  int start = g_strcmp0(a->start, b->start);
  return start ? start : g_utf8_collate(a->title, b->title);
}

void anto_calendar_load_events_file(GPtrArray *events, GHashTable *seen,
                                    const char *path) {
  struct json_object *root = json_object_from_file(path);
  if (!root || !json_object_is_type(root, json_type_array)) {
    if (root)
      json_object_put(root);
    return;
  }

  size_t count = json_object_array_length(root);
  for (size_t i = 0; i < count; i++) {
    struct json_object *object = json_object_array_get_idx(root, i);
    const char *date = anto_calendar_json_string(object, "date");
    const char *title = anto_calendar_json_string(object, "title");
    const char *start = anto_calendar_json_string(object, "start");
    if (strlen(date) < 10)
      continue;

    g_autofree char *key =
        g_strdup_printf("%.10s\x1f%s\x1f%s", date, start, title);
    if (g_hash_table_contains(seen, key))
      continue;
    g_hash_table_add(seen, g_strdup(key));

    CalendarEvent *event = g_new0(CalendarEvent, 1);
    event->id = g_strdup(anto_calendar_json_string(object, "id"));
    event->source = g_strdup(anto_calendar_json_string(object, "source"));
    event->date = g_strndup(date, 10);
    event->title = g_strdup(*title ? title : "Evento");
    event->start = g_strdup(start);
    event->end = g_strdup(anto_calendar_json_string(object, "end"));
    event->description =
        g_strdup(anto_calendar_json_string(object, "description"));
    struct json_object *all_day = NULL;
    event->all_day = json_object_object_get_ex(object, "all_day", &all_day) &&
                     json_object_get_boolean(all_day);
    g_ptr_array_add(events, event);
  }
  json_object_put(root);
}

GPtrArray *anto_calendar_read_events(void) {
  GPtrArray *events = g_ptr_array_new_with_free_func(anto_calendar_event_free);
  g_autoptr(GHashTable) seen =
      g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
  g_autofree char *local = g_build_filename(g_get_user_data_dir(), "anto426",
                                            "calendar", "events.json", NULL);
  g_autofree char *google = g_build_filename(
      g_get_user_data_dir(), "anto426", "calendar", "google_events.json", NULL);
  anto_calendar_load_events_file(events, seen, local);
  anto_calendar_load_events_file(events, seen, google);
  g_ptr_array_sort(events, anto_calendar_compare_events);
  return events;
}
