#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <json-c/json.h>
#include <poll.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define MAX_WORKSPACES 128
#define MAX_NAME 256

typedef enum {
    ROLE_FIRST,
    ROLE_CURRENT,
    ROLE_LAST,
    ROLE_INVALID,
} Role;

typedef struct {
    char name[MAX_NAME];
    int id;
    int windows;
    int slot;
    bool active;
} Workspace;

typedef struct {
    char output[MAX_NAME];
    char active_name[MAX_NAME];
    int active_id;
    Workspace workspaces[MAX_WORKSPACES];
    size_t count;
} WorkspaceState;

static volatile sig_atomic_t running = 1;

static void stop_running(int signal_number) {
    (void)signal_number;
    running = 0;
}

static const char *hyprctl_binary(void) {
    const char *override = getenv("ANTO_HYPRCTL_BIN");
    return override && *override ? override : "hyprctl";
}

static char *capture_command(char *const argv[]) {
    int descriptors[2];
    if (pipe(descriptors) != 0)
        return NULL;

    pid_t child = fork();
    if (child < 0) {
        close(descriptors[0]);
        close(descriptors[1]);
        return NULL;
    }

    if (child == 0) {
        int null_fd;
        close(descriptors[0]);
        if (dup2(descriptors[1], STDOUT_FILENO) < 0)
            _exit(126);
        close(descriptors[1]);
        null_fd = open("/dev/null", O_WRONLY);
        if (null_fd >= 0) {
            (void)dup2(null_fd, STDERR_FILENO);
            close(null_fd);
        }
        execvp(argv[0], argv);
        _exit(127);
    }

    close(descriptors[1]);
    size_t used = 0;
    size_t capacity = 8192;
    char *output = malloc(capacity);
    if (!output) {
        close(descriptors[0]);
        (void)waitpid(child, NULL, 0);
        return NULL;
    }

    for (;;) {
        if (used + 4096 + 1 > capacity) {
            size_t next_capacity = capacity * 2;
            char *next = realloc(output, next_capacity);
            if (!next) {
                free(output);
                close(descriptors[0]);
                (void)waitpid(child, NULL, 0);
                return NULL;
            }
            output = next;
            capacity = next_capacity;
        }

        ssize_t bytes = read(descriptors[0], output + used, capacity - used - 1);
        if (bytes > 0) {
            used += (size_t)bytes;
            continue;
        }
        if (bytes < 0 && errno == EINTR)
            continue;
        break;
    }

    close(descriptors[0]);
    int status = 0;
    if (waitpid(child, &status, 0) < 0 || !WIFEXITED(status) ||
        WEXITSTATUS(status) != 0) {
        free(output);
        return NULL;
    }
    output[used] = '\0';
    return output;
}

static int run_command(char *const argv[]) {
    pid_t child = fork();
    if (child < 0)
        return 1;
    if (child == 0) {
        execvp(argv[0], argv);
        _exit(127);
    }

    int status = 0;
    while (waitpid(child, &status, 0) < 0) {
        if (errno != EINTR)
            return 1;
    }
    return WIFEXITED(status) ? WEXITSTATUS(status) : 1;
}

static const char *json_string_member(struct json_object *object,
                                      const char *key) {
    struct json_object *value = NULL;
    if (!object || !json_object_object_get_ex(object, key, &value) ||
        !json_object_is_type(value, json_type_string))
        return "";
    return json_object_get_string(value);
}

static int json_int_member(struct json_object *object, const char *key) {
    struct json_object *value = NULL;
    if (!object || !json_object_object_get_ex(object, key, &value))
        return 0;
    return json_object_get_int(value);
}

static int workspace_slot(const char *name) {
    if (!name || !*name)
        return -1;
    static const char separator[] = "·";
    const char *start = name;
    const char *split = strstr(name, separator);
    if (split && split != name) {
        start = split + strlen(separator);
        if (!*start || strstr(start, separator))
            return -1;
    }
    for (const char *cursor = start; *cursor; ++cursor) {
        if (*cursor < '0' || *cursor > '9')
            return -1;
    }
    if (start[0] == '0')
        return -1;
    char *parsed_end = NULL;
    long value = strtol(start, &parsed_end, 10);
    if (!parsed_end || *parsed_end || value < 1 || value > 10)
        return -1;
    return (int)value;
}

static bool workspace_namespace_matches(const char *name,
                                        const char *reference) {
    static const char separator[] = "·";
    const char *left = name ? strstr(name, separator) : NULL;
    const char *right = reference ? strstr(reference, separator) : NULL;
    if (!left || !right)
        return true;
    size_t left_length = (size_t)(left - name);
    size_t right_length = (size_t)(right - reference);
    return left_length == right_length &&
           memcmp(name, reference, left_length) == 0;
}

static int compare_workspaces(const void *left_pointer,
                              const void *right_pointer) {
    const Workspace *left = left_pointer;
    const Workspace *right = right_pointer;
    if (left->slot >= 0 && right->slot >= 0 && left->slot != right->slot)
        return left->slot < right->slot ? -1 : 1;
    if (left->slot >= 0 && right->slot < 0)
        return -1;
    if (left->slot < 0 && right->slot >= 0)
        return 1;
    return strcmp(left->name, right->name);
}

static Workspace *workspace_with_slot(WorkspaceState *state, int slot) {
    for (size_t index = 0; index < state->count; ++index) {
        if (state->workspaces[index].slot == slot)
            return &state->workspaces[index];
    }
    return NULL;
}

static bool load_state(const char *requested_output, WorkspaceState *state) {
    memset(state, 0, sizeof(*state));

    char *monitor_argv[] = {(char *)hyprctl_binary(), "-j", "monitors", NULL};
    char *workspace_argv[] = {(char *)hyprctl_binary(), "-j", "workspaces",
                              NULL};
    char *monitor_text = capture_command(monitor_argv);
    char *workspace_text = capture_command(workspace_argv);
    if (!monitor_text || !workspace_text) {
        free(monitor_text);
        free(workspace_text);
        return false;
    }

    struct json_object *monitors = json_tokener_parse(monitor_text);
    struct json_object *workspaces = json_tokener_parse(workspace_text);
    free(monitor_text);
    free(workspace_text);
    if (!monitors || !workspaces ||
        !json_object_is_type(monitors, json_type_array) ||
        !json_object_is_type(workspaces, json_type_array)) {
        if (monitors)
            json_object_put(monitors);
        if (workspaces)
            json_object_put(workspaces);
        return false;
    }

    size_t monitor_count = json_object_array_length(monitors);
    for (size_t index = 0; index < monitor_count; ++index) {
        struct json_object *monitor = json_object_array_get_idx(monitors, index);
        const char *name = json_string_member(monitor, "name");
        struct json_object *focused_value = NULL;
        bool focused = json_object_object_get_ex(monitor, "focused",
                                                 &focused_value) &&
                       json_object_get_boolean(focused_value);
        if ((requested_output && *requested_output &&
             strcmp(name, requested_output) == 0) ||
            ((!requested_output || !*requested_output) && focused)) {
            snprintf(state->output, sizeof(state->output), "%s", name);
            struct json_object *active = NULL;
            if (json_object_object_get_ex(monitor, "activeWorkspace", &active)) {
                snprintf(state->active_name, sizeof(state->active_name), "%s",
                         json_string_member(active, "name"));
                state->active_id = json_int_member(active, "id");
            }
            break;
        }
    }
    json_object_put(monitors);

    if (!*state->output) {
        json_object_put(workspaces);
        return false;
    }

    size_t workspace_count = json_object_array_length(workspaces);
    int active_slot = workspace_slot(state->active_name);
    for (size_t index = 0;
         index < workspace_count && state->count < MAX_WORKSPACES; ++index) {
        struct json_object *workspace =
            json_object_array_get_idx(workspaces, index);
        const char *monitor = json_string_member(workspace, "monitor");
        const char *name = json_string_member(workspace, "name");
        int id = json_int_member(workspace, "id");
        int windows = json_int_member(workspace, "windows");
        bool active = strcmp(name, state->active_name) == 0;
        int slot = workspace_slot(name);

        if (strcmp(monitor, state->output) != 0 || !*name ||
            strncmp(name, "special:", 8) == 0 || (!active && windows <= 0) ||
            slot < 0 ||
            (active_slot >= 0 &&
             !workspace_namespace_matches(name, state->active_name)))
            continue;

        Workspace *duplicate = workspace_with_slot(state, slot);
        if (duplicate) {
            /* A stale alias for the same logical slot must never become a
             * fourth Waybar item.  Prefer the active entry if Hyprland emits
             * both during a workspace rename/move transition. */
            if (active && !duplicate->active) {
                snprintf(duplicate->name, sizeof(duplicate->name), "%s", name);
                duplicate->id = id;
                duplicate->windows = windows;
                duplicate->active = true;
            }
            continue;
        }

        Workspace *entry = &state->workspaces[state->count++];
        snprintf(entry->name, sizeof(entry->name), "%s", name);
        entry->id = id;
        entry->windows = windows;
        entry->slot = slot;
        entry->active = active;
    }
    json_object_put(workspaces);

    if (active_slot >= 0 && state->count < MAX_WORKSPACES &&
        strncmp(state->active_name, "special:", 8) != 0) {
        Workspace *entry = workspace_with_slot(state, active_slot);
        if (!entry) {
            entry = &state->workspaces[state->count++];
            entry->id = state->active_id;
        }
        if (!entry->active) {
            snprintf(entry->name, sizeof(entry->name), "%s",
                     state->active_name);
            entry->slot = active_slot;
            entry->active = true;
        }
    }

    qsort(state->workspaces, state->count, sizeof(state->workspaces[0]),
          compare_workspaces);
    return true;
}

static Role parse_role(const char *text) {
    if (text && strcmp(text, "first") == 0)
        return ROLE_FIRST;
    if (text && (strcmp(text, "current") == 0 || strcmp(text, "active") == 0))
        return ROLE_CURRENT;
    if (text && strcmp(text, "last") == 0)
        return ROLE_LAST;
    return ROLE_INVALID;
}

static const Workspace *active_workspace(const WorkspaceState *state) {
    for (size_t index = 0; index < state->count; ++index) {
        if (state->workspaces[index].active)
            return &state->workspaces[index];
    }
    return NULL;
}

/* The endpoint owns a duplicate active workspace. This keeps order stable:
 * first, current (only when internal), last. */
static const Workspace *workspace_for_role(const WorkspaceState *state,
                                           Role role) {
    if (!state || state->count == 0)
        return NULL;
    const Workspace *first = &state->workspaces[0];
    const Workspace *last = &state->workspaces[state->count - 1];
    const Workspace *current = active_workspace(state);

    switch (role) {
    case ROLE_FIRST:
        return first;
    case ROLE_CURRENT:
        if (!current || current == first || current == last)
            return NULL;
        return current;
    case ROLE_LAST:
        return last == first ? NULL : last;
    default:
        return NULL;
    }
}

static const char *role_name(Role role) {
    switch (role) {
    case ROLE_FIRST:
        return "first";
    case ROLE_CURRENT:
        return "current";
    case ROLE_LAST:
        return "last";
    default:
        return "invalid";
    }
}

static char *render_json(const char *output, Role role) {
    WorkspaceState state;
    struct json_object *root = json_object_new_object();
    struct json_object *classes = json_object_new_array();
    const Workspace *selected = NULL;

    if (load_state(output, &state))
        selected = workspace_for_role(&state, role);

    json_object_object_add(root, "text",
                           json_object_new_string(selected ? selected->name : ""));
    json_object_array_add(classes, json_object_new_string("workspace-triad-item"));
    json_object_array_add(classes, json_object_new_string(role_name(role)));

    if (selected) {
        char tooltip[1024];
        const Workspace *first = &state.workspaces[0];
        const Workspace *last = &state.workspaces[state.count - 1];
        const Workspace *current = active_workspace(&state);
        if (selected->active)
            json_object_array_add(classes, json_object_new_string("active"));
        snprintf(tooltip, sizeof(tooltip),
                 "%s · %s%s\nPrimo: %s  ·  Attuale: %s  ·  Ultimo: %s",
                 state.output, selected->name,
                 selected->active ? " · attivo" : "",
                 first->name, current ? current->name : "—", last->name);
        json_object_object_add(root, "tooltip",
                               json_object_new_string(tooltip));
    } else {
        json_object_array_add(classes, json_object_new_string("hidden"));
        json_object_object_add(root, "tooltip", json_object_new_string(""));
    }

    json_object_object_add(root, "class", classes);
    const char *serialized =
        json_object_to_json_string_ext(root, JSON_C_TO_STRING_PLAIN);
    char *result = strdup(serialized ? serialized : "{\"text\":\"\"}");
    json_object_put(root);
    return result;
}

static int connect_event_socket(void) {
    const char *override = getenv("ANTO_HYPR_EVENT_SOCKET");
    const char *signature = getenv("HYPRLAND_INSTANCE_SIGNATURE");
    const char *runtime = getenv("XDG_RUNTIME_DIR");
    char path[sizeof(((struct sockaddr_un *)0)->sun_path)] = {0};

    if (override && *override) {
        snprintf(path, sizeof(path), "%s", override);
    } else if (signature && *signature && runtime && *runtime) {
        snprintf(path, sizeof(path), "%s/hypr/%s/.socket2.sock", runtime,
                 signature);
        if (access(path, F_OK) != 0)
            snprintf(path, sizeof(path), "/tmp/hypr/%s/.socket2.sock",
                     signature);
    } else {
        return -1;
    }

    int descriptor = socket(AF_UNIX, SOCK_STREAM, 0);
    if (descriptor < 0)
        return -1;
    struct sockaddr_un address = {.sun_family = AF_UNIX};
    if (snprintf(address.sun_path, sizeof(address.sun_path), "%s", path) >=
        (int)sizeof(address.sun_path) ||
        connect(descriptor, (struct sockaddr *)&address, sizeof(address)) != 0) {
        close(descriptor);
        return -1;
    }
    return descriptor;
}

static bool relevant_events(const char *buffer, size_t length) {
    static const char *events[] = {
        "workspace>>",       "workspacev2>>",   "focusedmon>>",
        "createworkspace>>", "createworkspacev2>>",
        "destroyworkspace>>", "destroyworkspacev2>>",
        "moveworkspace>>",   "moveworkspacev2>>",
        "openwindow>>",      "closewindow>>",   "movewindow>>",
        "monitoradded>>",    "monitorremoved>>", "configreloaded>>",
    };
    for (size_t index = 0; index < sizeof(events) / sizeof(events[0]); ++index) {
        size_t event_length = strlen(events[index]);
        if (event_length > length)
            continue;
        for (size_t offset = 0; offset + event_length <= length; ++offset) {
            if (memcmp(buffer + offset, events[index], event_length) == 0)
                return true;
        }
    }
    return false;
}

static void sleep_milliseconds(long milliseconds) {
    struct timespec duration = {
        .tv_sec = milliseconds / 1000,
        .tv_nsec = (milliseconds % 1000) * 1000000L,
    };
    while (running && nanosleep(&duration, &duration) != 0 && errno == EINTR)
        ;
}

static void emit_if_changed(const char *output, Role role, char **previous) {
    char *json = render_json(output, role);
    if (!json)
        return;
    if (!*previous || strcmp(*previous, json) != 0) {
        puts(json);
        fflush(stdout);
        free(*previous);
        *previous = json;
    } else {
        free(json);
    }
}

static int stream_updates(const char *output, Role role) {
    char *previous = NULL;
    emit_if_changed(output, role, &previous);

    while (running) {
        int event_socket = connect_event_socket();
        if (event_socket < 0) {
            sleep_milliseconds(1000);
            emit_if_changed(output, role, &previous);
            continue;
        }

        while (running) {
            struct pollfd poll_descriptor = {
                .fd = event_socket,
                .events = POLLIN | POLLHUP | POLLERR,
            };
            int result = poll(&poll_descriptor, 1, 30000);
            if (result < 0 && errno == EINTR)
                continue;
            if (result < 0 || (result > 0 &&
                               (poll_descriptor.revents & (POLLHUP | POLLERR))))
                break;
            if (result == 0) {
                emit_if_changed(output, role, &previous);
                continue;
            }

            char event_buffer[16384];
            ssize_t bytes = read(event_socket, event_buffer, sizeof(event_buffer));
            if (bytes <= 0)
                break;
            if (relevant_events(event_buffer, (size_t)bytes)) {
                /* Let a short burst settle so create/focus/destroy cannot expose
                 * a transient fourth or empty workspace. */
                sleep_milliseconds(24);
                emit_if_changed(output, role, &previous);
            }
        }
        close(event_socket);
    }

    free(previous);
    return 0;
}

static int activate_role(const char *output, Role role) {
    WorkspaceState state;
    if (!load_state(output, &state))
        return 1;
    const Workspace *workspace = workspace_for_role(&state, role);
    if (!workspace)
        return 0;

    char target[MAX_NAME + 8];
    if (workspace->id > 0)
        snprintf(target, sizeof(target), "%d", workspace->id);
    else
        snprintf(target, sizeof(target), "name:%s", workspace->name);
    char *argv[] = {(char *)hyprctl_binary(), "dispatch", "workspace", target,
                    NULL};
    return run_command(argv);
}

static int cycle_workspace(const char *output, int direction) {
    WorkspaceState state;
    if (!load_state(output, &state) || state.count < 2)
        return state.count == 1 ? 0 : 1;

    size_t current_index = 0;
    for (size_t index = 0; index < state.count; ++index) {
        if (state.workspaces[index].active) {
            current_index = index;
            break;
        }
    }
    size_t target_index = direction > 0
                              ? (current_index + 1) % state.count
                              : (current_index + state.count - 1) % state.count;
    char target[MAX_NAME + 8];
    if (state.workspaces[target_index].id > 0)
        snprintf(target, sizeof(target), "%d", state.workspaces[target_index].id);
    else
        snprintf(target, sizeof(target), "name:%s",
                 state.workspaces[target_index].name);
    char *argv[] = {(char *)hyprctl_binary(), "dispatch", "workspace", target,
                    NULL};
    return run_command(argv);
}

static void usage(FILE *stream) {
    fprintf(stream,
            "Uso: workspace-triad render|stream|activate first|current|last\n"
            "     workspace-triad cycle previous|next\n"
            "Waybar imposta WAYBAR_OUTPUT_NAME per selezionare l'output.\n");
}

int main(int argc, char **argv) {
    if (argc != 3) {
        usage(stderr);
        return 2;
    }

    const char *output = getenv("WAYBAR_OUTPUT_NAME");
    Role role = parse_role(argv[2]);
    signal(SIGINT, stop_running);
    signal(SIGTERM, stop_running);
    signal(SIGPIPE, SIG_IGN);

    if (strcmp(argv[1], "cycle") == 0) {
        if (strcmp(argv[2], "next") == 0)
            return cycle_workspace(output, 1);
        if (strcmp(argv[2], "previous") == 0)
            return cycle_workspace(output, -1);
        usage(stderr);
        return 2;
    }
    if (role == ROLE_INVALID) {
        usage(stderr);
        return 2;
    }
    if (strcmp(argv[1], "render") == 0) {
        char *json = render_json(output, role);
        if (!json)
            return 1;
        puts(json);
        free(json);
        return 0;
    }
    if (strcmp(argv[1], "stream") == 0)
        return stream_updates(output, role);
    if (strcmp(argv[1], "activate") == 0)
        return activate_role(output, role);

    usage(stderr);
    return 2;
}
