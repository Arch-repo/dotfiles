#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <json-c/json.h>
#include <poll.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define DEFAULT_FRAME_MILLISECONDS 340L
#define DEFAULT_RETRY_MILLISECONDS 2000L
#define DEFAULT_MARQUEE_WIDTH 30L
#define MAX_INPUT_BYTES (256U * 1024U)

static const char metadata_separator = '\x1f';
static const char metadata_format[] =
    "{{status}}\x1f{{playerName}}\x1f{{title}}\x1f{{artist}}";
static const char marquee_gap[] = "        •        ";
static volatile sig_atomic_t running = 1;

typedef struct {
    char *status;
    char *player;
    char *title;
    char *artist;
    char *label;
    size_t offset;
} MediaState;

typedef struct {
    pid_t pid;
    int output_fd;
} PlayerProcess;

static void stop_running(int signal_number) {
    (void)signal_number;
    running = 0;
}

static size_t utf8_sequence_length(const unsigned char *text,
                                   size_t remaining) {
    if (!remaining)
        return 0;
    unsigned char first = text[0];
    if (first < 0x80)
        return 1;
    if (first >= 0xc2 && first <= 0xdf && remaining >= 2 &&
        (text[1] & 0xc0) == 0x80)
        return 2;
    if (first == 0xe0 && remaining >= 3 && text[1] >= 0xa0 &&
        text[1] <= 0xbf && (text[2] & 0xc0) == 0x80)
        return 3;
    if (((first >= 0xe1 && first <= 0xec) ||
         (first >= 0xee && first <= 0xef)) &&
        remaining >= 3 && (text[1] & 0xc0) == 0x80 &&
        (text[2] & 0xc0) == 0x80)
        return 3;
    if (first == 0xed && remaining >= 3 && text[1] >= 0x80 &&
        text[1] <= 0x9f && (text[2] & 0xc0) == 0x80)
        return 3;
    if (first == 0xf0 && remaining >= 4 && text[1] >= 0x90 &&
        text[1] <= 0xbf && (text[2] & 0xc0) == 0x80 &&
        (text[3] & 0xc0) == 0x80)
        return 4;
    if (first >= 0xf1 && first <= 0xf3 && remaining >= 4 &&
        (text[1] & 0xc0) == 0x80 && (text[2] & 0xc0) == 0x80 &&
        (text[3] & 0xc0) == 0x80)
        return 4;
    if (first == 0xf4 && remaining >= 4 && text[1] >= 0x80 &&
        text[1] <= 0x8f && (text[2] & 0xc0) == 0x80 &&
        (text[3] & 0xc0) == 0x80)
        return 4;
    return 0;
}

static char *utf8_sanitize(const char *input) {
    const unsigned char *source =
        (const unsigned char *)(input ? input : "");
    size_t length = strlen((const char *)source);
    if (length > (SIZE_MAX - 1) / 3)
        return NULL;
    char *result = malloc(length * 3 + 1);
    if (!result)
        return NULL;

    size_t input_offset = 0;
    size_t output_offset = 0;
    while (input_offset < length) {
        size_t sequence = utf8_sequence_length(
            source + input_offset, length - input_offset);
        if (!sequence) {
            result[output_offset++] = (char)0xef;
            result[output_offset++] = (char)0xbf;
            result[output_offset++] = (char)0xbd;
            input_offset++;
            continue;
        }
        memcpy(result + output_offset, source + input_offset, sequence);
        output_offset += sequence;
        input_offset += sequence;
    }
    result[output_offset] = '\0';
    return result;
}

static size_t utf8_character_count(const char *text) {
    size_t bytes = strlen(text ? text : "");
    size_t offset = 0;
    size_t characters = 0;
    while (offset < bytes) {
        size_t sequence = utf8_sequence_length(
            (const unsigned char *)text + offset, bytes - offset);
        offset += sequence ? sequence : 1;
        characters++;
    }
    return characters;
}

static size_t utf8_byte_offset(const char *text, size_t character) {
    size_t bytes = strlen(text ? text : "");
    size_t offset = 0;
    size_t current = 0;
    while (offset < bytes && current < character) {
        size_t sequence = utf8_sequence_length(
            (const unsigned char *)text + offset, bytes - offset);
        offset += sequence ? sequence : 1;
        current++;
    }
    return offset;
}

static char *join_text(const char *left, const char *middle,
                       const char *right) {
    size_t left_length = strlen(left ? left : "");
    size_t middle_length = strlen(middle ? middle : "");
    size_t right_length = strlen(right ? right : "");
    if (left_length > SIZE_MAX - middle_length ||
        left_length + middle_length > SIZE_MAX - right_length - 1)
        return NULL;
    size_t length = left_length + middle_length + right_length;
    char *result = malloc(length + 1);
    if (!result)
        return NULL;
    memcpy(result, left ? left : "", left_length);
    memcpy(result + left_length, middle ? middle : "", middle_length);
    memcpy(result + left_length + middle_length,
           right ? right : "", right_length);
    result[length] = '\0';
    return result;
}

static void media_state_clear(MediaState *state) {
    if (!state)
        return;
    free(state->status);
    free(state->player);
    free(state->title);
    free(state->artist);
    free(state->label);
    memset(state, 0, sizeof(*state));
}

static bool media_state_set(MediaState *state, const char *status,
                            const char *player, const char *title,
                            const char *artist) {
    char *next_status = utf8_sanitize(status && *status ? status : "Stopped");
    char *next_player = utf8_sanitize(player && *player ? player : "Media");
    char *next_title =
        utf8_sanitize(title && *title ? title : "Nessun media");
    char *next_artist = utf8_sanitize(artist ? artist : "");
    char *next_label = NULL;
    if (next_artist && *next_artist)
        next_label = join_text(next_title, "  •  ", next_artist);
    else if (next_title)
        next_label = strdup(next_title);
    if (!next_status || !next_player || !next_title || !next_artist ||
        !next_label) {
        free(next_status);
        free(next_player);
        free(next_title);
        free(next_artist);
        free(next_label);
        return false;
    }

    media_state_clear(state);
    state->status = next_status;
    state->player = next_player;
    state->title = next_title;
    state->artist = next_artist;
    state->label = next_label;
    return true;
}

static bool media_state_update_record(MediaState *state,
                                      const char *record) {
    char *copy = strdup(record ? record : "");
    if (!copy)
        return false;
    size_t length = strlen(copy);
    if (length && copy[length - 1] == '\r')
        copy[length - 1] = '\0';

    char *fields[4] = {copy, (char *)"", (char *)"", (char *)""};
    char *cursor = copy;
    for (size_t index = 0; index < 3; ++index) {
        char *separator = strchr(cursor, metadata_separator);
        if (!separator)
            break;
        *separator = '\0';
        cursor = separator + 1;
        fields[index + 1] = cursor;
    }
    bool result = media_state_set(state, fields[0], fields[1],
                                  fields[2], fields[3]);
    free(copy);
    return result;
}

static char *pango_escape(const char *input) {
    const char *text = input ? input : "";
    size_t length = strlen(text);
    if (length > (SIZE_MAX - 1) / 5)
        return NULL;
    char *result = malloc(length * 5 + 1);
    if (!result)
        return NULL;
    size_t output = 0;
    for (size_t index = 0; index < length; ++index) {
        const char *replacement = NULL;
        if (text[index] == '&')
            replacement = "&amp;";
        else if (text[index] == '<')
            replacement = "&lt;";
        else if (text[index] == '>')
            replacement = "&gt;";
        if (replacement) {
            size_t replacement_length = strlen(replacement);
            memcpy(result + output, replacement, replacement_length);
            output += replacement_length;
        } else {
            result[output++] = text[index];
        }
    }
    result[output] = '\0';
    return result;
}

static char *marquee_frame(MediaState *state, size_t width,
                           bool advance) {
    size_t label_characters = utf8_character_count(state->label);
    if (label_characters <= width)
        return strdup(state->label);

    char *source = join_text(state->label, marquee_gap, "");
    if (!source)
        return NULL;
    size_t source_characters = utf8_character_count(source);
    if (!source_characters) {
        free(source);
        return strdup("");
    }
    size_t start_character = state->offset % source_characters;
    size_t start_byte = utf8_byte_offset(source, start_character);
    size_t source_bytes = strlen(source);
    if (width > (SIZE_MAX - 1) / 4) {
        free(source);
        return NULL;
    }
    char *frame = malloc(width * 4 + 1);
    if (!frame) {
        free(source);
        return NULL;
    }

    size_t source_offset = start_byte;
    size_t frame_offset = 0;
    for (size_t character = 0; character < width; ++character) {
        if (source_offset >= source_bytes)
            source_offset = 0;
        size_t sequence = utf8_sequence_length(
            (const unsigned char *)source + source_offset,
            source_bytes - source_offset);
        if (!sequence)
            sequence = 1;
        memcpy(frame + frame_offset, source + source_offset, sequence);
        frame_offset += sequence;
        source_offset += sequence;
    }
    frame[frame_offset] = '\0';
    if (advance)
        state->offset = (start_character + 1) % source_characters;
    free(source);
    return frame;
}

static const char *status_class(const char *status) {
    if (status && strcasecmp(status, "Playing") == 0)
        return "playing";
    if (status && strcasecmp(status, "Paused") == 0)
        return "paused";
    return "idle";
}

static const char *status_icon(const char *status) {
    return status && strcasecmp(status, "Playing") == 0 ? "󰏤" : "󰐊";
}

static bool is_media_active(const char *status) {
    return status && (strcasecmp(status, "Playing") == 0 ||
                      strcasecmp(status, "Paused") == 0);
}

static bool emit_frame(MediaState *state, size_t width, bool advance) {
    bool active = is_media_active(state->status);
    char *safe_text = NULL;
    char *safe_tooltip = NULL;

    if (!active) {
        safe_text = strdup("");
        safe_tooltip = strdup("Nessun media in riproduzione");
    } else {
        char *frame = marquee_frame(state, width, advance);
        char *icon_and_frame =
            frame ? join_text(status_icon(state->status), "  ", frame) : NULL;
        char *tooltip = join_text(state->player, " · ", state->status);
        char *tooltip_with_artist = NULL;
        char *tooltip_complete = NULL;
        if (tooltip && state->artist && *state->artist)
            tooltip_with_artist = join_text(tooltip, "\n", state->artist);
        else if (tooltip)
            tooltip_with_artist = strdup(tooltip);
        if (tooltip_with_artist && state->title && *state->title)
            tooltip_complete = join_text(tooltip_with_artist, "\n", state->title);
        else if (tooltip_with_artist)
            tooltip_complete = strdup(tooltip_with_artist);
        safe_text = pango_escape(icon_and_frame);
        safe_tooltip = pango_escape(tooltip_complete);
        free(frame);
        free(icon_and_frame);
        free(tooltip);
        free(tooltip_with_artist);
        free(tooltip_complete);
    }
    if (!safe_text || !safe_tooltip) {
        free(safe_text);
        free(safe_tooltip);
        return false;
    }

    struct json_object *root = json_object_new_object();
    if (!root) {
        free(safe_text);
        free(safe_tooltip);
        return false;
    }
    json_object_object_add(root, "text", json_object_new_string(safe_text));
    json_object_object_add(root, "tooltip",
                           json_object_new_string(safe_tooltip));
    json_object_object_add(root, "class",
                           json_object_new_string(status_class(state->status)));
    const char *serialized =
        json_object_to_json_string_ext(root, JSON_C_TO_STRING_PLAIN);
    bool success = serialized && puts(serialized) >= 0 && fflush(stdout) == 0;
    json_object_put(root);
    free(safe_text);
    free(safe_tooltip);
    return success;
}

static long parse_long_value(const char *text, long fallback,
                             long minimum, long maximum) {
    if (!text || !*text)
        return fallback;
    errno = 0;
    char *end = NULL;
    long value = strtol(text, &end, 10);
    if (errno || !end || *end || value < minimum || value > maximum)
        return fallback;
    return value;
}

static long configured_frame_milliseconds(void) {
    const char *milliseconds = getenv("ANTO426_MEDIA_FRAME_MS");
    if (milliseconds && *milliseconds)
        return parse_long_value(milliseconds, DEFAULT_FRAME_MILLISECONDS,
                                20, 10000);
    const char *seconds = getenv("ANTO426_MEDIA_FRAME_SECONDS");
    if (!seconds || !*seconds)
        return DEFAULT_FRAME_MILLISECONDS;
    errno = 0;
    char *end = NULL;
    double value = strtod(seconds, &end);
    if (errno || !end || *end || value < 0.02 || value > 10.0)
        return DEFAULT_FRAME_MILLISECONDS;
    return (long)(value * 1000.0 + 0.5);
}

static int64_t monotonic_milliseconds(void) {
    struct timespec now = {0};
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0)
        return 0;
    return (int64_t)now.tv_sec * 1000 + now.tv_nsec / 1000000;
}

static void sleep_milliseconds(long milliseconds) {
    struct timespec duration = {
        .tv_sec = milliseconds / 1000,
        .tv_nsec = (milliseconds % 1000) * 1000000L,
    };
    while (running && nanosleep(&duration, &duration) != 0 && errno == EINTR)
        ;
}

static const char *playerctl_binary(void) {
    const char *override = getenv("ANTO426_PLAYERCTL");
    return override && *override ? override : "playerctl";
}

static bool spawn_playerctl(PlayerProcess *process) {
    int descriptors[2];
    if (pipe(descriptors) != 0)
        return false;
    pid_t child = fork();
    if (child < 0) {
        close(descriptors[0]);
        close(descriptors[1]);
        return false;
    }
    if (child == 0) {
        close(descriptors[0]);
        if (dup2(descriptors[1], STDOUT_FILENO) < 0)
            _exit(126);
        close(descriptors[1]);
        int null_fd = open("/dev/null", O_WRONLY);
        if (null_fd >= 0) {
            (void)dup2(null_fd, STDERR_FILENO);
            close(null_fd);
        }
        char *const argv[] = {
            (char *)playerctl_binary(), (char *)"--player=playerctld",
            (char *)"metadata", (char *)"--follow", (char *)"--format",
            (char *)metadata_format, NULL,
        };
        execvp(argv[0], argv);
        _exit(127);
    }

    close(descriptors[1]);
    int flags = fcntl(descriptors[0], F_GETFL, 0);
    if (flags >= 0)
        (void)fcntl(descriptors[0], F_SETFL, flags | O_NONBLOCK);
    process->pid = child;
    process->output_fd = descriptors[0];
    return true;
}

static void stop_player(PlayerProcess *process) {
    if (!process)
        return;
    if (process->output_fd >= 0) {
        close(process->output_fd);
        process->output_fd = -1;
    }
    if (process->pid > 0) {
        (void)kill(process->pid, SIGTERM);
        bool reaped = false;
        for (int attempt = 0; attempt < 50; ++attempt) {
            pid_t result = waitpid(process->pid, NULL, WNOHANG);
            if (result == process->pid || (result < 0 && errno == ECHILD)) {
                reaped = true;
                break;
            }
            if (result < 0 && errno != EINTR)
                break;
            struct timespec pause = {.tv_nsec = 10 * 1000 * 1000L};
            while (nanosleep(&pause, &pause) != 0 && errno == EINTR)
                ;
        }
        if (!reaped) {
            (void)kill(process->pid, SIGKILL);
            while (waitpid(process->pid, NULL, 0) < 0 && errno == EINTR)
                ;
        }
        process->pid = -1;
    }
}

static bool process_complete_records(MediaState *state, char *buffer,
                                     size_t *used, bool include_tail) {
    bool changed = false;
    size_t consumed = 0;
    while (consumed < *used) {
        char *newline = memchr(buffer + consumed, '\n', *used - consumed);
        if (!newline && !include_tail)
            break;
        size_t record_length = newline
                                   ? (size_t)(newline - (buffer + consumed))
                                   : *used - consumed;
        char saved = buffer[consumed + record_length];
        buffer[consumed + record_length] = '\0';
        if (media_state_update_record(state, buffer + consumed))
            changed = true;
        buffer[consumed + record_length] = saved;
        consumed += record_length + (newline ? 1U : 0U);
        if (!newline)
            break;
    }
    if (consumed) {
        memmove(buffer, buffer + consumed, *used - consumed);
        *used -= consumed;
    }
    return changed;
}

static bool frame_limit_reached(long maximum, long emitted) {
    return maximum > 0 && emitted >= maximum;
}

static int stream_media(void) {
    long frame_milliseconds = configured_frame_milliseconds();
    long retry_milliseconds = parse_long_value(
        getenv("ANTO426_MEDIA_RETRY_MS"), DEFAULT_RETRY_MILLISECONDS,
        10, 60000);
    long width = parse_long_value(getenv("ANTO426_MEDIA_MARQUEE_WIDTH"),
                                  DEFAULT_MARQUEE_WIDTH, 4, 160);
    long maximum_frames = parse_long_value(
        getenv("ANTO426_MEDIA_MAX_FRAMES"), 0, 0, 1000000);
    MediaState state = {0};
    if (!media_state_set(&state, "Stopped", "Media", "Nessun media", ""))
        return 1;

    long emitted = 0;
    while (running) {
        if (!emit_frame(&state, (size_t)width, true)) {
            media_state_clear(&state);
            return 0;
        }
        emitted++;
        if (frame_limit_reached(maximum_frames, emitted))
            break;

        PlayerProcess player = {.pid = -1, .output_fd = -1};
        if (!spawn_playerctl(&player)) {
            sleep_milliseconds(retry_milliseconds);
            continue;
        }

        char *input = malloc(MAX_INPUT_BYTES + 1);
        if (!input) {
            stop_player(&player);
            media_state_clear(&state);
            return 1;
        }
        size_t used = 0;
        int64_t next_frame =
            monotonic_milliseconds() + frame_milliseconds;
        bool child_closed = false;
        while (running && !child_closed) {
            int64_t now = monotonic_milliseconds();
            int64_t remaining = next_frame - now;
            int timeout = remaining <= 0
                              ? 0
                              : remaining > 60000 ? 60000 : (int)remaining;
            struct pollfd descriptor = {
                .fd = player.output_fd,
                .events = POLLIN | POLLHUP | POLLERR,
            };
            int poll_result = poll(&descriptor, 1, timeout);
            if (poll_result < 0 && errno == EINTR)
                continue;
            if (poll_result < 0)
                break;

            bool metadata_changed = false;
            if (poll_result > 0 &&
                (descriptor.revents & (POLLIN | POLLHUP | POLLERR))) {
                for (;;) {
                    if (used == MAX_INPUT_BYTES) {
                        used = 0;
                    }
                    ssize_t bytes = read(player.output_fd, input + used,
                                         MAX_INPUT_BYTES - used);
                    if (bytes > 0) {
                        used += (size_t)bytes;
                        input[used] = '\0';
                        if (process_complete_records(&state, input, &used,
                                                     false))
                            metadata_changed = true;
                        continue;
                    }
                    if (bytes == 0) {
                        if (used && process_complete_records(
                                        &state, input, &used, true))
                            metadata_changed = true;
                        child_closed = true;
                        break;
                    }
                    if (errno == EINTR)
                        continue;
                    if (errno == EAGAIN || errno == EWOULDBLOCK)
                        break;
                    child_closed = true;
                    break;
                }
            }

            now = monotonic_milliseconds();
            if (metadata_changed || now >= next_frame) {
                if (!emit_frame(&state, (size_t)width, true)) {
                    running = 0;
                    break;
                }
                emitted++;
                if (frame_limit_reached(maximum_frames, emitted)) {
                    running = 0;
                    break;
                }
                next_frame = now + (is_media_active(state.status) ? frame_milliseconds : 1000L);
            }
        }
        free(input);
        stop_player(&player);
        if (running)
            sleep_milliseconds(retry_milliseconds);
    }

    media_state_clear(&state);
    return 0;
}

static int render_once(int argc, char **argv) {
    if (argc != 6 && argc != 7)
        return 2;
    long width = parse_long_value(getenv("ANTO426_MEDIA_MARQUEE_WIDTH"),
                                  DEFAULT_MARQUEE_WIDTH, 4, 160);
    long offset = argc == 7
                      ? parse_long_value(argv[6], 0, 0, 1000000)
                      : 0;
    MediaState state = {0};
    if (!media_state_set(&state, argv[2], argv[3], argv[4], argv[5]))
        return 1;
    state.offset = (size_t)offset;
    bool success = emit_frame(&state, (size_t)width, false);
    media_state_clear(&state);
    return success ? 0 : 1;
}

static void usage(FILE *stream) {
    fprintf(stream,
            "Uso: media-marquee stream\n"
            "     media-marquee render STATUS PLAYER TITLE ARTIST [OFFSET]\n");
}

int main(int argc, char **argv) {
    signal(SIGINT, stop_running);
    signal(SIGTERM, stop_running);
    signal(SIGPIPE, SIG_IGN);
    if (argc == 2 && strcmp(argv[1], "stream") == 0)
        return stream_media();
    if (argc >= 2 && strcmp(argv[1], "render") == 0) {
        int result = render_once(argc, argv);
        if (result == 2)
            usage(stderr);
        return result;
    }
    usage(stderr);
    return 2;
}
