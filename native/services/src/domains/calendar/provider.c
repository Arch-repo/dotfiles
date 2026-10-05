#include "backend.h"
#include "service.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

#define CALENDAR_INPUT_LIMIT (64U * 1024U)

typedef struct {
    char *directory;
    char *events;
    char *lock;
    char *sync;
    int lock_fd;
} CalendarStore;

static void store_clear(CalendarStore *store) {
    if (!store) return;
    if (store->lock_fd >= 0) close(store->lock_fd);
    g_free(store->directory);
    g_free(store->events);
    g_free(store->lock);
    g_free(store->sync);
    memset(store, 0, sizeof(*store));
    store->lock_fd = -1;
}

static CalendarStore store_paths(void) {
    CalendarStore store = {.lock_fd = -1};
    const char *events_override = g_getenv("ANTO_CALENDAR_EVENTS_FILE");
    if (events_override && *events_override) {
        store.events = g_strdup(events_override);
        store.directory = g_path_get_dirname(events_override);
    } else {
        store.directory = g_build_filename(g_get_user_data_dir(), "anto426",
                                           "calendar", NULL);
        store.events = g_build_filename(store.directory, "events.json", NULL);
    }
    const char *runtime = g_get_user_runtime_dir();
    if (!runtime || !*runtime) runtime = g_get_tmp_dir();
    g_autofree char *lock_name = g_strdup_printf(
        "anto426-calendar-local-%u.lock", (unsigned)getuid());
    const char *lock_override = g_getenv("ANTO_CALENDAR_LOCK_FILE");
    store.lock = lock_override && *lock_override
                     ? g_strdup(lock_override)
                     : g_build_filename(runtime, lock_name, NULL);
    const char *sync_override = g_getenv("ANTO_CALENDAR_SYNC_SCRIPT");
    store.sync = sync_override && *sync_override
                     ? g_strdup(sync_override)
                     : g_build_filename(g_get_user_config_dir(), "anto426",
                                        "remote_sync.sh", NULL);
    return store;
}

static int calendar_error(const char *code, const char *message) {
    return backend_error(1, code, message);
}

static const char *error_message(const GError *error) {
    return error && error->message ? error->message : "Operazione calendario non riuscita";
}

static gboolean lock_store(CalendarStore *store, GError **error) {
    store->lock_fd = g_open(store->lock, O_RDWR | O_CREAT, 0600);
    if (store->lock_fd < 0) {
        g_set_error(error, G_FILE_ERROR, g_file_error_from_errno(errno),
                    "Impossibile aprire il lock calendario: %s",
                    g_strerror(errno));
        return FALSE;
    }
    (void)fcntl(store->lock_fd, F_SETFD, FD_CLOEXEC);
    if (flock(store->lock_fd, LOCK_EX) != 0) {
        g_set_error(error, G_FILE_ERROR, g_file_error_from_errno(errno),
                    "Impossibile bloccare l'archivio calendario: %s",
                    g_strerror(errno));
        return FALSE;
    }
    return TRUE;
}

static gboolean valid_string_member(json_object *entry, const char *name) {
    json_object *value = NULL;
    if (!json_object_object_get_ex(entry, name, &value) || !value) return TRUE;
    return json_object_is_type(value, json_type_string);
}

static gboolean validate_store(json_object *root) {
    if (!root || !json_object_is_type(root, json_type_array)) return FALSE;
    const size_t length = json_object_array_length(root);
    for (size_t index = 0; index < length; index++) {
        json_object *entry = json_object_array_get_idx(root, index);
        if (!entry || !json_object_is_type(entry, json_type_object) ||
            !valid_string_member(entry, "date") ||
            !valid_string_member(entry, "title") ||
            !valid_string_member(entry, "start") ||
            !valid_string_member(entry, "end") ||
            !valid_string_member(entry, "description"))
            return FALSE;
    }
    return TRUE;
}

static json_object *read_store(CalendarStore *store, GError **error) {
    if (g_mkdir_with_parents(store->directory, 0700) != 0) {
        g_set_error(error, G_FILE_ERROR, g_file_error_from_errno(errno),
                    "Impossibile creare l'archivio calendario: %s",
                    g_strerror(errno));
        return NULL;
    }
    if (!g_file_test(store->events, G_FILE_TEST_EXISTS)) {
        if (!g_file_set_contents(store->events, "[]\n", -1, error)) return NULL;
        (void)g_chmod(store->events, 0600);
    }
    g_autofree char *contents = NULL;
    gsize size = 0;
    if (!g_file_get_contents(store->events, &contents, &size, error)) return NULL;
    if (size > (gsize)INT_MAX) {
        g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_NO_SPACE,
                            "Archivio calendario troppo grande");
        return NULL;
    }
    json_tokener *tokener = json_tokener_new();
    if (!tokener) {
        g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_NO_SPACE,
                            "Memoria insufficiente per leggere il calendario");
        return NULL;
    }
    json_object *root = json_tokener_parse_ex(tokener, contents, (int)size);
    enum json_tokener_error parse_error = json_tokener_get_error(tokener);
    json_tokener_free(tokener);
    if (parse_error != json_tokener_success || !validate_store(root)) {
        if (root) json_object_put(root);
        g_set_error(error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA,
                    "Archivio locale non valido: %s", store->events);
        return NULL;
    }
    return root;
}

static gboolean atomic_write_json(const char *path, json_object *root, GError **error) {
    const char *serialized = json_object_to_json_string_ext(root, JSON_C_TO_STRING_PRETTY | JSON_C_TO_STRING_SPACED);
    g_autofree char *text = g_strconcat(serialized, "\n", NULL);
    return backend_write_atomic(path, text, 0600, error);
}


static const char *json_text(json_object *object, const char *name) {
    json_object *value = NULL;
    if (!object || !json_object_object_get_ex(object, name, &value) ||
        !value || !json_object_is_type(value, json_type_string))
        return "";
    const char *text = json_object_get_string(value);
    return text ? text : "";
}

static gboolean json_member_bool(json_object *object, const char *name) {
    json_object *value = NULL;
    return object && json_object_object_get_ex(object, name, &value) && value &&
           json_object_is_type(value, json_type_boolean) &&
           json_object_get_boolean(value);
}

static gboolean valid_date(const char *date) {
    if (!date || strlen(date) != 10 || date[4] != '-' || date[7] != '-')
        return FALSE;
    for (guint i = 0; i < 10; i++)
        if (i != 4 && i != 7 && !g_ascii_isdigit(date[i])) return FALSE;
    int year = (date[0] - '0') * 1000 + (date[1] - '0') * 100 +
               (date[2] - '0') * 10 + date[3] - '0';
    int month = (date[5] - '0') * 10 + date[6] - '0';
    int day = (date[8] - '0') * 10 + date[9] - '0';
    g_autoptr(GDateTime) parsed = g_date_time_new_local(
        year, month, day, 12, 0, 0);
    return parsed && g_date_time_get_year(parsed) == year &&
           g_date_time_get_month(parsed) == month &&
           g_date_time_get_day_of_month(parsed) == day;
}

static gboolean valid_time(const char *time) {
    if (!time || strlen(time) != 5 || time[2] != ':' ||
        !g_ascii_isdigit(time[0]) || !g_ascii_isdigit(time[1]) ||
        !g_ascii_isdigit(time[3]) || !g_ascii_isdigit(time[4]))
        return FALSE;
    int hour = (time[0] - '0') * 10 + time[1] - '0';
    int minute = (time[3] - '0') * 10 + time[4] - '0';
    return hour <= 23 && minute <= 59;
}

static char *read_stdin_limited(GError **error) {
    GString *input = g_string_sized_new(1024);
    char buffer[4096];
    while (!feof(stdin)) {
        size_t count = fread(buffer, 1, sizeof(buffer), stdin);
        if (count > 0) {
            if (input->len + count > CALENDAR_INPUT_LIMIT) {
                g_string_free(input, TRUE);
                g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_NO_SPACE,
                                    "Dati evento troppo grandi");
                return NULL;
            }
            g_string_append_len(input, buffer, (gssize)count);
        }
        if (ferror(stdin)) {
            g_string_free(input, TRUE);
            g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_FAILED,
                                "Impossibile leggere i dati evento");
            return NULL;
        }
    }
    return g_string_free(input, FALSE);
}

static int compare_event_objects(const void *left, const void *right) {
    json_object *a = *(json_object *const *)left;
    json_object *b = *(json_object *const *)right;
    int value = g_strcmp0(json_text(a, "date"), json_text(b, "date"));
    if (value) return value;
    const char *a_time = json_member_bool(a, "all_day") ? "00:00" : json_text(a, "start");
    const char *b_time = json_member_bool(b, "all_day") ? "00:00" : json_text(b, "start");
    value = g_strcmp0(a_time, b_time);
    return value ? value : g_strcmp0(json_text(a, "title"),
                                    json_text(b, "title"));
}

static void sort_events(json_object *events) {
    const size_t length = json_object_array_length(events);
    if (length < 2) return;
    json_object **items = g_new(json_object *, length);
    for (size_t i = 0; i < length; i++)
        items[i] = json_object_get(json_object_array_get_idx(events, i));
    qsort(items, length, sizeof(*items), compare_event_objects);
    while (json_object_array_length(events) > 0)
        json_object_array_del_idx(events, 0, 1);
    for (size_t i = 0; i < length; i++)
        json_object_array_add(events, items[i]);
    g_free(items);
}

static int action_add(CalendarStore *store) {
    g_autoptr(GError) error = NULL;
    g_autofree char *payload = read_stdin_limited(&error);
    if (!payload) return calendar_error("input", error_message(error));
    json_object *event = json_tokener_parse(payload);
    if (!event || !json_object_is_type(event, json_type_object)) {
        if (event) json_object_put(event);
        return calendar_error("invalid-event", "Dati evento non validi");
    }
    g_autofree char *title = backend_clean_field(json_text(event, "title"));
    const char *date = json_text(event, "date");
    const char *start = json_text(event, "start");
    const char *end = json_text(event, "end");
    const char *description = json_text(event, "description");
    gboolean all_day = json_member_bool(event, "all_day");
    if (!*title) {
        json_object_put(event);
        return calendar_error("title", "Il titolo e' obbligatorio");
    }
    if (strlen(title) > 180 || strlen(description) > 4000) {
        json_object_put(event);
        return calendar_error("length", "Titolo o descrizione troppo lunghi");
    }
    if (!valid_date(date)) {
        json_object_put(event);
        return calendar_error("date", "Data non valida");
    }
    if (!all_day && (!valid_time(start) || (*end && !valid_time(end)) ||
                     (*end && g_strcmp0(end, start) <= 0))) {
        json_object_put(event);
        return calendar_error("time", "Orario evento non valido");
    }
    if (!lock_store(store, &error)) {
        json_object_put(event);
        return calendar_error("lock", error_message(error));
    }
    json_object *events = read_store(store, &error);
    if (!events) {
        json_object_put(event);
        return calendar_error("store", error_message(error));
    }
    g_autofree char *uuid = g_uuid_string_random();
    g_autoptr(GDateTime) now = g_date_time_new_now_local();
    g_autofree char *created = g_date_time_format_iso8601(now);
    g_autofree char *identifier = g_strdup_printf(
        "local-%" G_GINT64_FORMAT "-%s", g_get_real_time() / G_USEC_PER_SEC,
        uuid);
    json_object *saved = json_object_new_object();
    json_object_object_add(saved, "id", json_object_new_string(identifier));
    json_object_object_add(saved, "source", json_object_new_string("local"));
    json_object_object_add(saved, "date", json_object_new_string(date));
    json_object_object_add(saved, "start",
                           json_object_new_string(all_day ? "" : start));
    json_object_object_add(saved, "end",
                           json_object_new_string(all_day ? "" : end));
    json_object_object_add(saved, "title", json_object_new_string(title));
    json_object_object_add(saved, "description",
                           json_object_new_string(description));
    json_object_object_add(saved, "all_day", json_object_new_boolean(all_day));
    json_object_object_add(saved, "created_at",
                           json_object_new_string(created ? created : ""));
    json_object_object_add(saved, "sync_state", json_object_new_string("local"));
    json_object_array_add(events, saved);
    sort_events(events);
    gboolean written = atomic_write_json(store->events, events, &error);
    json_object_put(events);
    json_object_put(event);
    if (!written) return calendar_error("write", error_message(error));
    g_print("%s\n", identifier);
    g_autofree char *message = g_strdup_printf(
        "%s · %s%s%s", title, date,
        !all_day && *start ? " alle " : "", !all_day ? start : "");
    backend_notify("Evento salvato", message);
    return 0;
}

static int action_delete(CalendarStore *store, const char *identifier) {
    if (!identifier || !g_str_has_prefix(identifier, "local-") ||
        strlen(identifier) > 128)
        return calendar_error("invalid-id", "ID evento locale non valido");
    g_autoptr(GError) error = NULL;
    if (!lock_store(store, &error)) return calendar_error("lock", error_message(error));
    json_object *events = read_store(store, &error);
    if (!events) return calendar_error("store", error_message(error));
    gboolean found = FALSE;
    for (size_t i = json_object_array_length(events); i > 0; i--) {
        json_object *entry = json_object_array_get_idx(events, i - 1);
        if (g_strcmp0(json_text(entry, "id"), identifier) == 0) {
            json_object_array_del_idx(events, i - 1, 1);
            found = TRUE;
        }
    }
    if (!found) {
        json_object_put(events);
        return calendar_error("not-found", "Evento locale non trovato");
    }
    gboolean written = atomic_write_json(store->events, events, &error);
    json_object_put(events);
    return written ? 0 : calendar_error("write", error_message(error));
}

static int action_list(CalendarStore *store) {
    g_autoptr(GError) error = NULL;
    if (!lock_store(store, &error)) return calendar_error("lock", error_message(error));
    json_object *events = read_store(store, &error);
    if (!events) return calendar_error("store", error_message(error));
    g_print("%s\n", json_object_to_json_string_ext(
                        events, JSON_C_TO_STRING_PRETTY | JSON_C_TO_STRING_SPACED));
    json_object_put(events);
    return 0;
}

static int action_sync(CalendarStore *store) {
    g_autoptr(GError) error = NULL;
    if (!lock_store(store, &error)) return calendar_error("lock", error_message(error));
    json_object *events = read_store(store, &error);
    if (!events) return calendar_error("store", error_message(error));
    json_object_put(events);
    (void)flock(store->lock_fd, LOCK_UN);
    close(store->lock_fd);
    store->lock_fd = -1;
    if (!g_file_test(store->sync, G_FILE_TEST_IS_EXECUTABLE))
        return calendar_error("sync-unavailable",
                              "Sincronizzazione remota non disponibile");
    const char *argv[] = {store->sync, "calendar", NULL};
    return backend_command_forward(argv, NULL);
}

int anto_calendar_execute(int argc, char **argv) {
    (void)argc;

    extern const BackendService anto_service_calendar;
    CalendarStore store = store_paths();
    int status = 2;
    switch (backend_operation_index(&anto_service_calendar, argv[0])) {
        case 0: status = action_list(&store); break;
        case 1: status = action_add(&store); break;
        case 2: status = action_delete(&store, argv[1]); break;
        case 3: status = action_sync(&store); break;
    }
    store_clear(&store);
    return status;
}
