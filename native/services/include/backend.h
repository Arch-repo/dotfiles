#pragma once

#include <gio/gio.h>
#include <glib/gstdio.h>
#include <json-c/json.h>

typedef struct {
    char *stdout_text;
    char *stderr_text;
    int status;
    gboolean started;
} BackendCommand;

BackendCommand backend_run_program(const char *environment, const char *fallback, const char *const arguments[]);
gboolean backend_write_atomic(const char *path, const char *text, gint mode, GError **error);
char *backend_first_line(const char *text);
const char *backend_program(const char *environment, const char *fallback);
gboolean backend_dry_run(void);
gboolean backend_no_notify(void);
gboolean backend_valid_address(const char *address);
gboolean backend_valid_token(const char *value);
char *backend_clean_field(const char *value);
void backend_print_field(const char *value);
void backend_command_clear(BackendCommand *command);
BackendCommand backend_command_run(const char *const argv[],
                                   const char *stdin_text);
BackendCommand backend_command_run_bytes(const char *const argv[],
                                         GBytes *stdin_bytes,
                                         GBytes **stdout_bytes);
int backend_command_forward(const char *const argv[], const char *stdin_text);
gboolean backend_spawn_detached(const char *const argv[], GPid *pid,
                                GError **error);
void backend_notify(const char *title, const char *body);
int backend_error(int status, const char *code, const char *message);
int backend_usage(const char *domain, const char *usage);

int backend_network_main(int argc, char **argv);
int backend_bluetooth_main(int argc, char **argv);
int backend_bluez_snapshot(void);
int backend_bluez_mutation(int argc, char **argv);
int backend_bluez_scan_worker(const char *address, const char *duration);
int backend_audio_main(int argc, char **argv);
int backend_energy_main(int argc, char **argv);
int backend_system_main(int argc, char **argv);
int backend_session_main(int argc, char **argv);
int backend_capture_main(int argc, char **argv);
int backend_record_main(int argc, char **argv);
int backend_config_main(int argc, char **argv);
int backend_notes_main(int argc, char **argv);
int backend_calendar_main(int argc, char **argv);
int backend_display_main(int argc, char **argv);
