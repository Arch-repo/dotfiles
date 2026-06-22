#define _GNU_SOURCE

#include <ctype.h>
#include <glib.h>
#include <json-c/json.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char *value;
    char *tzid;
    gboolean value_date;
} DateSpec;

typedef struct {
    DateSpec dtstart;
    DateSpec dtend;
    char *uid;
    char *summary;
    char *description;
    char *location;
    char *url;
    char *rrule;
} IcsEvent;

typedef struct {
    char *id;
    char *date;
    char *start;
    char *end;
    char *title;
    char *description;
    char *url;
    gboolean all_day;
} Occurrence;

typedef struct {
    GDateTime *dt;
    gboolean all_day;
} ParsedDate;

static void free_date_spec(DateSpec *spec) {
    if (!spec) return;
    g_free(spec->value);
    g_free(spec->tzid);
}

static void free_ics_event(gpointer data) {
    IcsEvent *event = data;
    if (!event) return;
    free_date_spec(&event->dtstart);
    free_date_spec(&event->dtend);
    g_free(event->uid);
    g_free(event->summary);
    g_free(event->description);
    g_free(event->location);
    g_free(event->url);
    g_free(event->rrule);
    g_free(event);
}

static void free_occurrence(gpointer data) {
    Occurrence *occ = data;
    if (!occ) return;
    g_free(occ->id);
    g_free(occ->date);
    g_free(occ->start);
    g_free(occ->end);
    g_free(occ->title);
    g_free(occ->description);
    g_free(occ->url);
    g_free(occ);
}

static GTimeZone *new_timezone(const char *name) {
    GTimeZone *tz = NULL;
    if (name && *name) tz = g_time_zone_new_identifier(name);
    if (!tz) tz = g_time_zone_new_local();
    return tz;
}

static gboolean parse_digits(const char *text, int pos, int len, int *out) {
    int value = 0;

    for (int i = 0; i < len; i++) {
        unsigned char c = (unsigned char)text[pos + i];
        if (!g_ascii_isdigit(c)) return FALSE;
        value = value * 10 + (c - '0');
    }

    *out = value;
    return TRUE;
}

static gboolean looks_like_date(const char *value) {
    if (!value || strlen(value) < 8) return FALSE;
    for (int i = 0; i < 8; i++) {
        if (!g_ascii_isdigit((unsigned char)value[i])) return FALSE;
    }
    return value[8] == '\0';
}

static ParsedDate parse_datetime(const DateSpec *spec, GTimeZone *local_tz) {
    ParsedDate parsed = {0};
    int year = 0, month = 0, day = 0, hour = 0, minute = 0, second = 0;
    const char *value = spec ? spec->value : NULL;
    gboolean is_utc = FALSE;
    gsize len;

    if (!value || strlen(value) < 8 ||
        !parse_digits(value, 0, 4, &year) ||
        !parse_digits(value, 4, 2, &month) ||
        !parse_digits(value, 6, 2, &day)) {
        return parsed;
    }

    len = strlen(value);
    parsed.all_day = (spec && spec->value_date) || looks_like_date(value);

    if (!parsed.all_day && len >= 13) {
        if (!parse_digits(value, 9, 2, &hour) ||
            !parse_digits(value, 11, 2, &minute)) {
            return parsed;
        }
        if (len >= 15 && !parse_digits(value, 13, 2, &second)) return parsed;
        is_utc = value[len - 1] == 'Z';
    }

    if (parsed.all_day) {
        parsed.dt = g_date_time_new(local_tz, year, month, day, 0, 0, 0);
        return parsed;
    }

    if (is_utc) {
        GDateTime *utc = g_date_time_new_utc(year, month, day, hour, minute, second);
        if (utc) {
            parsed.dt = g_date_time_to_timezone(utc, local_tz);
            g_date_time_unref(utc);
        }
    } else {
        GTimeZone *tz = (spec && spec->tzid) ? new_timezone(spec->tzid) : g_time_zone_ref(local_tz);
        GDateTime *dt = g_date_time_new(tz, year, month, day, hour, minute, second);
        if (dt) {
            parsed.dt = g_date_time_to_timezone(dt, local_tz);
            g_date_time_unref(dt);
        }
        g_time_zone_unref(tz);
    }

    return parsed;
}

static char *unescape_text(const char *value) {
    GString *out = g_string_new(NULL);

    for (const char *p = value ? value : ""; *p; p++) {
        if (*p == '\\' && p[1]) {
            p++;
            switch (*p) {
                case 'n':
                case 'N':
                    g_string_append_c(out, ' ');
                    break;
                case ',':
                case ';':
                case '\\':
                    g_string_append_c(out, *p);
                    break;
                default:
                    g_string_append_c(out, *p);
                    break;
            }
        } else {
            g_string_append_c(out, *p);
        }
    }

    return g_strstrip(g_string_free(out, FALSE));
}

static char *clean_text(const char *value, int limit) {
    GString *out = g_string_new(NULL);
    gboolean last_space = TRUE;

    for (const char *p = value ? value : ""; *p; p = g_utf8_next_char(p)) {
        gunichar ch = g_utf8_get_char_validated(p, -1);
        if (ch == (gunichar)-1 || ch == (gunichar)-2) {
            ch = (unsigned char)*p;
        }

        if (g_unichar_isspace(ch)) {
            if (!last_space) g_string_append_c(out, ' ');
            last_space = TRUE;
        } else {
            g_string_append_unichar(out, ch);
            last_space = FALSE;
        }
    }

    while (out->len > 0 && out->str[out->len - 1] == ' ') {
        g_string_truncate(out, out->len - 1);
    }

    if (limit > 3 && g_utf8_strlen(out->str, -1) > limit) {
        char *end = g_utf8_offset_to_pointer(out->str, limit - 3);
        g_string_truncate(out, (gsize)(end - out->str));
        while (out->len > 0 && out->str[out->len - 1] == ' ') {
            g_string_truncate(out, out->len - 1);
        }
        g_string_append(out, "...");
    }

    return g_string_free(out, FALSE);
}

static GPtrArray *unfold_ics_lines(const char *content) {
    GPtrArray *lines = g_ptr_array_new_with_free_func(g_free);
    GString *current = NULL;
    char **raw_lines = g_strsplit(content ? content : "", "\n", -1);

    for (int i = 0; raw_lines[i]; i++) {
        char *line = g_strdup(raw_lines[i]);
        g_strchomp(line);

        if ((line[0] == ' ' || line[0] == '\t') && current) {
            g_string_append(current, line + 1);
        } else {
            if (current) g_ptr_array_add(lines, g_string_free(current, FALSE));
            current = g_string_new(line);
        }
        g_free(line);
    }

    if (current) g_ptr_array_add(lines, g_string_free(current, FALSE));
    g_strfreev(raw_lines);
    return lines;
}

static char *strip_quotes(char *text) {
    char *trimmed = g_strstrip(text);
    gsize len = strlen(trimmed);
    if (len >= 2 && trimmed[0] == '"' && trimmed[len - 1] == '"') {
        trimmed[len - 1] = '\0';
        trimmed++;
    }
    return trimmed;
}

static void parse_property(const char *line, char **name_out, GHashTable **params_out, char **value_out) {
    const char *colon = strchr(line, ':');
    char *left;
    char **parts;

    *name_out = NULL;
    *params_out = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
    *value_out = NULL;

    if (!colon) return;

    left = g_strndup(line, (gsize)(colon - line));
    *value_out = g_strdup(colon + 1);
    parts = g_strsplit(left, ";", -1);

    if (parts[0]) *name_out = g_ascii_strup(parts[0], -1);

    for (int i = 1; parts[i]; i++) {
        char *eq = strchr(parts[i], '=');
        if (!eq) continue;
        char *key = g_ascii_strup(parts[i], (gssize)(eq - parts[i]));
        char *val = g_strdup(strip_quotes(eq + 1));
        g_hash_table_insert(*params_out, key, val);
    }

    g_strfreev(parts);
    g_free(left);
}

static void set_date_spec(DateSpec *spec, const char *value, GHashTable *params) {
    const char *value_type = g_hash_table_lookup(params, "VALUE");
    const char *tzid = g_hash_table_lookup(params, "TZID");

    g_free(spec->value);
    g_free(spec->tzid);
    spec->value = g_strdup(value);
    spec->tzid = tzid ? g_strdup(tzid) : NULL;
    spec->value_date = value_type && g_ascii_strcasecmp(value_type, "DATE") == 0;
}

static GPtrArray *parse_ics_events(const char *ics_path) {
    char *content = NULL;
    GPtrArray *events = g_ptr_array_new_with_free_func(free_ics_event);
    GPtrArray *lines;
    IcsEvent *current = NULL;

    if (!g_file_get_contents(ics_path, &content, NULL, NULL)) return events;
    lines = unfold_ics_lines(content);

    for (guint i = 0; i < lines->len; i++) {
        const char *line = g_ptr_array_index(lines, i);
        char *name = NULL;
        char *value = NULL;
        GHashTable *params = NULL;

        parse_property(line, &name, &params, &value);
        if (!name) {
            g_hash_table_unref(params);
            g_free(value);
            continue;
        }

        if (g_strcmp0(name, "BEGIN") == 0 && g_strcmp0(value, "VEVENT") == 0) {
            current = g_new0(IcsEvent, 1);
        } else if (g_strcmp0(name, "END") == 0 && g_strcmp0(value, "VEVENT") == 0 && current) {
            g_ptr_array_add(events, current);
            current = NULL;
        } else if (current) {
            if (g_strcmp0(name, "DTSTART") == 0) {
                set_date_spec(&current->dtstart, value, params);
            } else if (g_strcmp0(name, "DTEND") == 0) {
                set_date_spec(&current->dtend, value, params);
            } else if (g_strcmp0(name, "UID") == 0) {
                g_free(current->uid);
                current->uid = g_strdup(value);
            } else if (g_strcmp0(name, "SUMMARY") == 0) {
                g_free(current->summary);
                current->summary = g_strdup(value);
            } else if (g_strcmp0(name, "DESCRIPTION") == 0) {
                g_free(current->description);
                current->description = g_strdup(value);
            } else if (g_strcmp0(name, "LOCATION") == 0) {
                g_free(current->location);
                current->location = g_strdup(value);
            } else if (g_strcmp0(name, "URL") == 0) {
                g_free(current->url);
                current->url = g_strdup(value);
            } else if (g_strcmp0(name, "RRULE") == 0) {
                g_free(current->rrule);
                current->rrule = g_strdup(value);
            }
        }

        g_free(name);
        g_free(value);
        g_hash_table_unref(params);
    }

    if (current) free_ics_event(current);
    g_ptr_array_unref(lines);
    g_free(content);
    return events;
}

static GHashTable *parse_rrule(const char *rrule) {
    GHashTable *rule = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
    char **parts = g_strsplit(rrule ? rrule : "", ";", -1);

    for (int i = 0; parts[i]; i++) {
        char *eq = strchr(parts[i], '=');
        if (!eq) continue;
        g_hash_table_insert(rule,
            g_ascii_strup(parts[i], (gssize)(eq - parts[i])),
            g_strdup(eq + 1));
    }

    g_strfreev(parts);
    return rule;
}

static int rule_int(GHashTable *rule, const char *key, int fallback) {
    const char *value = g_hash_table_lookup(rule, key);
    char *end = NULL;
    long parsed;
    if (!value || !*value) return fallback;
    parsed = strtol(value, &end, 10);
    if (!end || end == value) return fallback;
    return (int)parsed;
}

static GDateTime *parse_until(const char *value, GTimeZone *local_tz) {
    DateSpec spec = {0};
    ParsedDate parsed;
    GDateTime *until = NULL;

    if (!value || !*value) return NULL;
    spec.value = (char *)value;
    spec.value_date = looks_like_date(value);
    parsed = parse_datetime(&spec, local_tz);
    if (!parsed.dt) return NULL;

    if (parsed.all_day) {
        until = g_date_time_new(local_tz,
            g_date_time_get_year(parsed.dt),
            g_date_time_get_month(parsed.dt),
            g_date_time_get_day_of_month(parsed.dt),
            23, 59, 59);
        g_date_time_unref(parsed.dt);
    } else {
        until = parsed.dt;
    }

    return until;
}

static int days_in_month(int year, int month) {
    static const int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month == 2 && ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0)) return 29;
    return days[month - 1];
}

static GDateTime *same_time_on_date(GDateTime *time_source, GDateTime *date_source, GTimeZone *local_tz) {
    return g_date_time_new(local_tz,
        g_date_time_get_year(date_source),
        g_date_time_get_month(date_source),
        g_date_time_get_day_of_month(date_source),
        g_date_time_get_hour(time_source),
        g_date_time_get_minute(time_source),
        g_date_time_get_second(time_source));
}

static GDateTime *add_months_clamped(GDateTime *dt, int months, GTimeZone *local_tz) {
    int year = g_date_time_get_year(dt);
    int month = g_date_time_get_month(dt) - 1 + months;
    int day = g_date_time_get_day_of_month(dt);

    year += month / 12;
    month %= 12;
    if (month < 0) {
        month += 12;
        year--;
    }
    month++;
    day = MIN(day, days_in_month(year, month));

    return g_date_time_new(local_tz, year, month, day,
        g_date_time_get_hour(dt),
        g_date_time_get_minute(dt),
        g_date_time_get_second(dt));
}

static gboolean occurrence_allowed(GDateTime *candidate, GDateTime *until) {
    return !until || g_date_time_compare(candidate, until) <= 0;
}

static void add_candidate(GPtrArray *starts, GDateTime *candidate, GDateTime *start, GDateTime *until) {
    if (g_date_time_compare(candidate, start) >= 0 && occurrence_allowed(candidate, until)) {
        g_ptr_array_add(starts, g_date_time_ref(candidate));
    }
}

static GPtrArray *expand_starts(GDateTime *start, GHashTable *rule, GDateTime *window_end, GTimeZone *local_tz) {
    GPtrArray *starts = g_ptr_array_new_with_free_func((GDestroyNotify)g_date_time_unref);
    const char *freq = g_hash_table_lookup(rule, "FREQ");
    int interval = MAX(rule_int(rule, "INTERVAL", 1), 1);
    int count_limit = rule_int(rule, "COUNT", 1000);
    int max_seen = MIN(MAX(count_limit, 1), 1500);
    GDateTime *until = parse_until(g_hash_table_lookup(rule, "UNTIL"), local_tz);

    if (!freq || !*freq) {
        g_ptr_array_add(starts, g_date_time_ref(start));
        if (until) g_date_time_unref(until);
        return starts;
    }

    if (g_ascii_strcasecmp(freq, "DAILY") == 0) {
        GDateTime *current = g_date_time_ref(start);
        for (int seen = 0; seen < max_seen && g_date_time_compare(current, window_end) <= 0; seen++) {
            add_candidate(starts, current, start, until);
            GDateTime *next = g_date_time_add_days(current, interval);
            g_date_time_unref(current);
            current = next;
        }
        g_date_time_unref(current);
    } else if (g_ascii_strcasecmp(freq, "WEEKLY") == 0) {
        const char *byday_text = g_hash_table_lookup(rule, "BYDAY");
        int bydays[7];
        int byday_count = 0;
        int start_weekday = g_date_time_get_day_of_week(start) - 1;
        GDateTime *week_start = g_date_time_add_days(start, -start_weekday);

        if (byday_text && *byday_text) {
            char **tokens = g_strsplit(byday_text, ",", -1);
            for (int i = 0; tokens[i] && byday_count < 7; i++) {
                if (g_strcmp0(tokens[i], "MO") == 0) bydays[byday_count++] = 0;
                else if (g_strcmp0(tokens[i], "TU") == 0) bydays[byday_count++] = 1;
                else if (g_strcmp0(tokens[i], "WE") == 0) bydays[byday_count++] = 2;
                else if (g_strcmp0(tokens[i], "TH") == 0) bydays[byday_count++] = 3;
                else if (g_strcmp0(tokens[i], "FR") == 0) bydays[byday_count++] = 4;
                else if (g_strcmp0(tokens[i], "SA") == 0) bydays[byday_count++] = 5;
                else if (g_strcmp0(tokens[i], "SU") == 0) bydays[byday_count++] = 6;
            }
            g_strfreev(tokens);
        }
        if (byday_count == 0) bydays[byday_count++] = start_weekday;

        for (int seen = 0; seen < max_seen && g_date_time_compare(week_start, window_end) <= 0; seen += byday_count) {
            for (int i = 0; i < byday_count; i++) {
                GDateTime *date = g_date_time_add_days(week_start, bydays[i]);
                GDateTime *candidate = same_time_on_date(start, date, local_tz);
                add_candidate(starts, candidate, start, until);
                g_date_time_unref(candidate);
                g_date_time_unref(date);
            }
            GDateTime *next = g_date_time_add_weeks(week_start, interval);
            g_date_time_unref(week_start);
            week_start = next;
        }
        g_date_time_unref(week_start);
    } else if (g_ascii_strcasecmp(freq, "MONTHLY") == 0) {
        const char *bymonthday_text = g_hash_table_lookup(rule, "BYMONTHDAY");
        int monthdays[31];
        int monthday_count = 0;
        GDateTime *current = g_date_time_ref(start);

        if (bymonthday_text && *bymonthday_text) {
            char **tokens = g_strsplit(bymonthday_text, ",", -1);
            for (int i = 0; tokens[i] && monthday_count < 31; i++) {
                int day = atoi(tokens[i]);
                if (day > 0 && day <= 31) monthdays[monthday_count++] = day;
            }
            g_strfreev(tokens);
        }

        for (int seen = 0; seen < max_seen && g_date_time_compare(current, window_end) <= 0; seen += MAX(monthday_count, 1)) {
            if (monthday_count == 0) {
                add_candidate(starts, current, start, until);
            } else {
                for (int i = 0; i < monthday_count; i++) {
                    int year = g_date_time_get_year(current);
                    int month = g_date_time_get_month(current);
                    if (monthdays[i] <= days_in_month(year, month)) {
                        GDateTime *candidate = g_date_time_new(local_tz, year, month, monthdays[i],
                            g_date_time_get_hour(start),
                            g_date_time_get_minute(start),
                            g_date_time_get_second(start));
                        add_candidate(starts, candidate, start, until);
                        g_date_time_unref(candidate);
                    }
                }
            }
            GDateTime *next = add_months_clamped(current, interval, local_tz);
            g_date_time_unref(current);
            current = next;
        }
        g_date_time_unref(current);
    } else if (g_ascii_strcasecmp(freq, "YEARLY") == 0) {
        int current_year = g_date_time_get_year(start);
        int seen = 0;
        while (seen < max_seen && current_year <= g_date_time_get_year(window_end)) {
            int month = g_date_time_get_month(start);
            int day = g_date_time_get_day_of_month(start);
            if (day <= days_in_month(current_year, month)) {
                GDateTime *candidate = g_date_time_new(local_tz, current_year, month, day,
                    g_date_time_get_hour(start),
                    g_date_time_get_minute(start),
                    g_date_time_get_second(start));
                add_candidate(starts, candidate, start, until);
                g_date_time_unref(candidate);
            }
            current_year += interval;
            seen++;
        }
    } else {
        g_ptr_array_add(starts, g_date_time_ref(start));
    }

    if (until) g_date_time_unref(until);
    return starts;
}

static char *format_date(GDateTime *dt) {
    return g_date_time_format(dt, "%Y-%m-%d");
}

static char *format_time(GDateTime *dt) {
    return g_date_time_format(dt, "%H:%M");
}

static char *format_iso(GDateTime *dt) {
    return g_date_time_format(dt, "%Y-%m-%dT%H:%M:%S%z");
}

static void add_occurrence(GPtrArray *out, const char *uid, GDateTime *start, GDateTime *end,
                           gboolean all_day, const char *title, const char *description, const char *url) {
    Occurrence *occ = g_new0(Occurrence, 1);
    char *key_time = all_day ? format_date(start) : format_iso(start);

    occ->id = g_strdup_printf("gcal-%s-%s", uid && *uid ? uid : title, key_time);
    occ->date = format_date(start);
    occ->start = all_day ? g_strdup("") : format_time(start);
    occ->end = all_day ? g_strdup("") : format_time(end);
    occ->title = g_strdup(title && *title ? title : "Event");
    occ->description = g_strdup(description ? description : "");
    occ->url = g_strdup(url ? url : "");
    occ->all_day = all_day;

    g_ptr_array_add(out, occ);
    g_free(key_time);
}

static gint occurrence_compare(gconstpointer a, gconstpointer b) {
    const Occurrence *oa = *(Occurrence * const *)a;
    const Occurrence *ob = *(Occurrence * const *)b;
    const char *sa = (oa->start && *oa->start) ? oa->start : "00:00";
    const char *sb = (ob->start && *ob->start) ? ob->start : "00:00";
    int cmp = g_strcmp0(oa->date, ob->date);
    if (cmp != 0) return cmp;
    cmp = g_strcmp0(sa, sb);
    if (cmp != 0) return cmp;
    return g_strcmp0(oa->title, ob->title);
}

static json_object *occurrence_to_json(const Occurrence *occ) {
    json_object *obj = json_object_new_object();
    json_object_object_add(obj, "id", json_object_new_string(occ->id));
    json_object_object_add(obj, "source", json_object_new_string("google"));
    json_object_object_add(obj, "date", json_object_new_string(occ->date));
    json_object_object_add(obj, "start", json_object_new_string(occ->start));
    json_object_object_add(obj, "end", json_object_new_string(occ->end));
    json_object_object_add(obj, "title", json_object_new_string(occ->title));
    json_object_object_add(obj, "description", json_object_new_string(occ->description));
    json_object_object_add(obj, "url", json_object_new_string(occ->url));
    json_object_object_add(obj, "all_day", json_object_new_boolean(occ->all_day));
    return obj;
}

static int write_json_events(const char *out_path, GPtrArray *occurrences) {
    json_object *array = json_object_new_array();
    const char *json_text;
    FILE *fh;

    g_ptr_array_sort(occurrences, occurrence_compare);
    for (guint i = 0; i < occurrences->len; i++) {
        json_object_array_add(array, occurrence_to_json(g_ptr_array_index(occurrences, i)));
    }

    fh = fopen(out_path, "w");
    if (!fh) {
        json_object_put(array);
        return 1;
    }

    json_text = json_object_to_json_string_ext(array, JSON_C_TO_STRING_PRETTY | JSON_C_TO_STRING_SPACED);
    fputs(json_text, fh);
    fputc('\n', fh);
    fclose(fh);
    json_object_put(array);
    return 0;
}

static int sync_calendar(int argc, char **argv) {
    const char *ics_path;
    const char *out_path;
    int past_days;
    int future_days;
    GTimeZone *local_tz;
    GDateTime *now;
    GDateTime *today_start;
    GDateTime *window_start;
    GDateTime *window_end;
    GPtrArray *events;
    GPtrArray *occurrences;
    int rc;

    if (argc != 7) {
        fprintf(stderr, "usage: %s sync-calendar <ics> <out.json> <past-days> <future-days> <timezone>\n", argv[0]);
        return 2;
    }

    ics_path = argv[2];
    out_path = argv[3];
    past_days = atoi(argv[4]);
    future_days = atoi(argv[5]);
    local_tz = new_timezone(argv[6]);
    now = g_date_time_new_now(local_tz);
    today_start = g_date_time_new(local_tz,
        g_date_time_get_year(now), g_date_time_get_month(now), g_date_time_get_day_of_month(now),
        0, 0, 0);
    window_start = g_date_time_add_days(today_start, -MAX(past_days, 0));
    window_end = g_date_time_add_days(today_start, MAX(future_days, 0) + 1);

    events = parse_ics_events(ics_path);
    occurrences = g_ptr_array_new_with_free_func(free_occurrence);

    for (guint i = 0; i < events->len; i++) {
        IcsEvent *event = g_ptr_array_index(events, i);
        ParsedDate start;
        ParsedDate end = {0};
        GDateTime *end_dt = NULL;
        GTimeSpan duration;
        GHashTable *rule;
        GPtrArray *starts;
        char *title;
        char *description;
        char *location;
        char *uid;
        char *url;

        if (!event->dtstart.value) continue;

        start = parse_datetime(&event->dtstart, local_tz);
        if (!start.dt) continue;

        if (event->dtend.value) end = parse_datetime(&event->dtend, local_tz);
        if (end.dt) {
            end_dt = end.dt;
        } else if (start.all_day) {
            end_dt = g_date_time_add_days(start.dt, 1);
        } else {
            end_dt = g_date_time_add_hours(start.dt, 1);
        }

        duration = g_date_time_difference(end_dt, start.dt);
        if (duration < G_TIME_SPAN_MINUTE) duration = G_TIME_SPAN_MINUTE;

        title = unescape_text(event->summary ? event->summary : "Event");
        description = unescape_text(event->description ? event->description : "");
        location = unescape_text(event->location ? event->location : "");
        uid = unescape_text(event->uid ? event->uid : title);
        url = unescape_text(event->url ? event->url : "");

        if (*location && !strstr(description, location)) {
            char *merged = g_strdup_printf("%s%sLocation: %s", description, *description ? "  " : "", location);
            g_free(description);
            description = merged;
        }

        rule = parse_rrule(event->rrule);
        starts = expand_starts(start.dt, rule, window_end, local_tz);

        for (guint j = 0; j < starts->len; j++) {
            GDateTime *occ_start = g_ptr_array_index(starts, j);
            GDateTime *occ_end = g_date_time_add(occ_start, duration);

            if (g_date_time_compare(occ_end, window_start) < 0 ||
                g_date_time_compare(occ_start, window_end) > 0) {
                g_date_time_unref(occ_end);
                continue;
            }

            if (start.all_day) {
                GDateTime *current_day = g_date_time_new(local_tz,
                    g_date_time_get_year(occ_start),
                    g_date_time_get_month(occ_start),
                    g_date_time_get_day_of_month(occ_start),
                    0, 0, 0);
                GDateTime *last_second = g_date_time_add_seconds(occ_end, -1);
                GDateTime *last_day = g_date_time_new(local_tz,
                    g_date_time_get_year(last_second),
                    g_date_time_get_month(last_second),
                    g_date_time_get_day_of_month(last_second),
                    0, 0, 0);

                while (g_date_time_compare(current_day, last_day) <= 0) {
                    if (g_date_time_compare(current_day, window_start) >= 0 &&
                        g_date_time_compare(current_day, window_end) <= 0) {
                        GDateTime *day_end = g_date_time_add_days(current_day, 1);
                        add_occurrence(occurrences, uid, current_day, day_end, TRUE, title, description, url);
                        g_date_time_unref(day_end);
                    }
                    GDateTime *next_day = g_date_time_add_days(current_day, 1);
                    g_date_time_unref(current_day);
                    current_day = next_day;
                }
                g_date_time_unref(current_day);
                g_date_time_unref(last_day);
                g_date_time_unref(last_second);
            } else {
                add_occurrence(occurrences, uid, occ_start, occ_end, FALSE, title, description, url);
            }

            g_date_time_unref(occ_end);
        }

        g_ptr_array_unref(starts);
        g_hash_table_unref(rule);
        g_free(title);
        g_free(description);
        g_free(location);
        g_free(uid);
        g_free(url);
        if (end.dt) g_date_time_unref(end.dt);
        else g_date_time_unref(end_dt);
        g_date_time_unref(start.dt);
    }

    rc = write_json_events(out_path, occurrences);

    g_ptr_array_unref(occurrences);
    g_ptr_array_unref(events);
    g_date_time_unref(window_end);
    g_date_time_unref(window_start);
    g_date_time_unref(today_start);
    g_date_time_unref(now);
    g_time_zone_unref(local_tz);
    return rc;
}

static const char *json_get_string(json_object *obj, const char *key, const char *fallback) {
    json_object *value = NULL;
    if (!obj || !json_object_object_get_ex(obj, key, &value) || json_object_get_type(value) == json_type_null) {
        return fallback;
    }
    return json_object_get_string(value);
}

static gboolean json_get_bool(json_object *obj, const char *key) {
    json_object *value = NULL;
    if (!obj || !json_object_object_get_ex(obj, key, &value)) return FALSE;
    return json_object_get_boolean(value);
}

static GHashTable *read_state_set(const char *state_path) {
    GHashTable *set = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
    char *content = NULL;

    if (g_file_get_contents(state_path, &content, NULL, NULL)) {
        char **lines = g_strsplit(content, "\n", -1);
        for (int i = 0; lines[i]; i++) {
            char *line = g_strstrip(lines[i]);
            if (*line) g_hash_table_add(set, g_strdup(line));
        }
        g_strfreev(lines);
    }

    g_free(content);
    return set;
}

static gint string_ptr_compare(gconstpointer a, gconstpointer b) {
    const char *sa = *(char * const *)a;
    const char *sb = *(char * const *)b;
    return g_strcmp0(sa, sb);
}

static void write_state_set(const char *state_path, GHashTable *kept) {
    GPtrArray *keys = g_ptr_array_new();
    GHashTableIter iter;
    gpointer key;
    GString *out = g_string_new(NULL);
    char *dir = g_path_get_dirname(state_path);

    g_hash_table_iter_init(&iter, kept);
    while (g_hash_table_iter_next(&iter, &key, NULL)) {
        g_ptr_array_add(keys, key);
    }
    g_ptr_array_sort(keys, string_ptr_compare);

    for (guint i = 0; i < keys->len; i++) {
        g_string_append(out, g_ptr_array_index(keys, i));
        g_string_append_c(out, '\n');
    }

    g_mkdir_with_parents(dir, 0700);
    g_file_set_contents(state_path, out->str, (gssize)out->len, NULL);
    g_free(dir);
    g_string_free(out, TRUE);
    g_ptr_array_unref(keys);
}

static GDateTime *datetime_for_event(const char *date_text, const char *start_text, gboolean all_day, GTimeZone *local_tz) {
    int year = 0, month = 0, day = 0, hour = 9, minute = 0;

    if (!date_text || strlen(date_text) < 10 ||
        !parse_digits(date_text, 0, 4, &year) ||
        !parse_digits(date_text, 5, 2, &month) ||
        !parse_digits(date_text, 8, 2, &day)) {
        return NULL;
    }

    if (!all_day) {
        if (!start_text || strlen(start_text) < 5 ||
            !parse_digits(start_text, 0, 2, &hour) ||
            !parse_digits(start_text, 3, 2, &minute)) {
            return NULL;
        }
    }

    return g_date_time_new(local_tz, year, month, day, hour, minute, 0);
}

static int due_notifications(int argc, char **argv) {
    const char *events_path;
    const char *state_path;
    int lookback;
    GTimeZone *local_tz;
    GDateTime *now;
    GDateTime *recent_cutoff;
    GHashTable *sent;
    GHashTable *recent = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
    GHashTable *new_keys = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
    GHashTable *kept = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
    json_object *root;

    if (argc != 6) {
        fprintf(stderr, "usage: %s due <events.json> <state-file> <lookback-seconds> <timezone>\n", argv[0]);
        return 2;
    }

    events_path = argv[2];
    state_path = argv[3];
    lookback = atoi(argv[4]);
    if (lookback < 60) lookback = 60;
    if (lookback > 7200) lookback = 7200;
    local_tz = new_timezone(argv[5]);
    now = g_date_time_new_now(local_tz);
    recent_cutoff = g_date_time_add_days(now, -3);
    sent = read_state_set(state_path);
    root = json_object_from_file(events_path);

    if (root && json_object_get_type(root) == json_type_array) {
        size_t len = json_object_array_length(root);
        for (size_t i = 0; i < len; i++) {
            json_object *event = json_object_array_get_idx(root, i);
            gboolean all_day = json_get_bool(event, "all_day");
            char *date_text = clean_text(json_get_string(event, "date", ""), 20);
            char *start_text = clean_text(json_get_string(event, "start", ""), 12);
            char *end_text = clean_text(json_get_string(event, "end", ""), 12);
            char *title = clean_text(json_get_string(event, "title", "Event"), 120);
            char *description = clean_text(json_get_string(event, "description", ""), 160);
            const char *id = json_get_string(event, "id", "");
            GDateTime *start_dt = datetime_for_event(date_text, start_text, all_day, local_tz);

            if (start_dt) {
                char *key = g_strdup_printf("%s|%s|%s",
                    id && *id ? id : title,
                    date_text,
                    all_day || !*start_text ? "all-day" : start_text);

                if (g_date_time_compare(start_dt, recent_cutoff) >= 0) {
                    g_hash_table_add(recent, g_strdup(key));
                }

                GTimeSpan elapsed = g_date_time_difference(now, start_dt);
                if (!g_hash_table_contains(sent, key) &&
                    elapsed >= 0 &&
                    elapsed <= (GTimeSpan)lookback * G_TIME_SPAN_SECOND) {
                    char *start_label = all_day ? g_strdup("All day") :
                        ((*end_text) ? g_strdup_printf("%s-%s", start_text, end_text) : g_strdup(start_text));
                    char *body = *description ?
                        g_strdup_printf("%s - %s - %s", start_label, title, description) :
                        g_strdup_printf("%s - %s", start_label, title);
                    char *clean_body = clean_text(body, 500);

                    printf("Calendar\t%s\n", clean_body);
                    g_hash_table_add(new_keys, g_strdup(key));
                    g_free(clean_body);
                    g_free(body);
                    g_free(start_label);
                }

                g_free(key);
                g_date_time_unref(start_dt);
            }

            g_free(date_text);
            g_free(start_text);
            g_free(end_text);
            g_free(title);
            g_free(description);
        }
    }

    GHashTableIter iter;
    gpointer key;
    g_hash_table_iter_init(&iter, sent);
    while (g_hash_table_iter_next(&iter, &key, NULL)) {
        if (g_hash_table_contains(recent, key)) g_hash_table_add(kept, g_strdup(key));
    }
    g_hash_table_iter_init(&iter, new_keys);
    while (g_hash_table_iter_next(&iter, &key, NULL)) {
        g_hash_table_add(kept, g_strdup(key));
    }
    write_state_set(state_path, kept);

    if (root) json_object_put(root);
    g_hash_table_unref(kept);
    g_hash_table_unref(new_keys);
    g_hash_table_unref(recent);
    g_hash_table_unref(sent);
    g_date_time_unref(recent_cutoff);
    g_date_time_unref(now);
    g_time_zone_unref(local_tz);
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s <sync-calendar|due> ...\n", argv[0]);
        return 2;
    }

    if (g_strcmp0(argv[1], "sync-calendar") == 0) return sync_calendar(argc, argv);
    if (g_strcmp0(argv[1], "due") == 0) return due_notifications(argc, argv);

    fprintf(stderr, "unknown command: %s\n", argv[1]);
    return 2;
}
