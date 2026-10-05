#pragma once
/* Internal display domain contract; the CLI is declared by service.c. */
#include "backend.h"
#include "service.h"

typedef struct {
    char *state_root;
    char *profile_dir;
    char *persistent_file;
    char *virtual_state;
    char *virtual_backend;
    char *lock_file;
    gboolean dry_run;
    gboolean mock_apply;
    gboolean mutation_applied;
    int lock_fd;
} DisplayContext;

typedef gboolean (*DisplayMutation)(DisplayContext *context,
                                    json_object *monitors,
                                    const char *first,
                                    const char *second,
                                    GError **error);

gboolean anto_display_acquire_lock(DisplayContext *context, GError **error);
int anto_display_action_arrange(DisplayContext *context, const char *request);
int anto_display_action_dpms(DisplayContext *context, json_object *monitors,
                       const char *verb, const char *name);
int anto_display_action_editor(void);
int anto_display_action_focus(DisplayContext *context, json_object *monitors,
                        const char *name);
int anto_display_action_help(void);
int anto_display_action_list(json_object *monitors);
int anto_display_action_modes(json_object *monitors, const char *name);
int anto_display_action_persist_current(DisplayContext *context,
                                  json_object *monitors);
int anto_display_action_persistent_preview(DisplayContext *context,
                                     json_object *monitors);
int anto_display_action_profile_delete(DisplayContext *context, const char *name);
int anto_display_action_profile_list(DisplayContext *context);
int anto_display_action_profile_save(DisplayContext *context, json_object *monitors,
                               const char *name);
int anto_display_action_status(json_object *monitors);
guint anto_display_active_count(json_object *monitors);
gboolean anto_display_apply_output(DisplayContext *context, const char *name,
                             const char *mode, const char *position,
                             const char *scale, gint64 transform,
                             const char *mirror, GError **error);
DisplayContext anto_display_display_context(void);
void anto_display_display_context_clear(DisplayContext *context);
int anto_display_display_error(const char *code, const char *message);
const char *anto_display_display_error_message(const GError *error);
json_object *anto_display_find_output(json_object *monitors, const char *name);
char *anto_display_format_number(double value);
int anto_display_guarded_named(DisplayContext *context, const char *label,
                         DisplayMutation mutation, const char *first,
                         const char *second);
json_object *anto_display_load_json_file(const char *path, GError **error);
gboolean anto_display_member_boolean(json_object *object, const char *name,
                               gboolean fallback);
double anto_display_member_double(json_object *object, const char *name,
                            double fallback);
gint64 anto_display_member_integer(json_object *object, const char *name,
                             gint64 fallback);
const char *anto_display_member_string(json_object *object, const char *name,
                                 const char *fallback);
const char *anto_display_mirror_for(json_object *monitor);
gboolean anto_display_mode_available(json_object *monitor, const char *requested);
char *anto_display_mode_for(json_object *monitor);
json_object *anto_display_monitor_json(GError **error);
json_object *anto_display_parse_json_text(const char *text, GError **error);
gboolean anto_display_parse_mode(const char *mode, int *width, int *height,
                           double *rate);
gboolean anto_display_parse_scale(const char *text, double *scale);
gboolean anto_display_persist_snapshot(DisplayContext *context,
                                 json_object *monitors, GError **error);
char *anto_display_position_for(json_object *monitor);
const char *anto_display_primary_output(json_object *monitors);
gboolean anto_display_require_output(json_object *monitors, const char *name,
                               GError **error);
gboolean anto_display_restore_snapshot(DisplayContext *context, json_object *profile,
                                 GError **error);
gboolean anto_display_run_hypr(DisplayContext *context, const char *first,
                         const char *second, const char *third,
                         GError **error);
gboolean anto_display_save_quick_profiles(DisplayContext *context,
                                    json_object *monitors,
                                    gboolean both, GError **error);
gboolean anto_display_scale_fits_mode(const char *mode, const char *scale_text);
json_object *anto_display_snapshot_copy(json_object *monitors);
gboolean anto_display_valid_direction(const char *direction);
gboolean anto_display_valid_output_name(const char *name);
gboolean anto_display_valid_profile_name(const char *name);
gboolean anto_display_validate_snapshot(json_object *monitors, GError **error);
gboolean anto_display_virtual_names(DisplayContext *context, GHashTable **names,
                              GError **error);
gboolean anto_display_mutation_profile(DisplayContext *context,
                                 json_object *monitors, const char *name,
                                 const char *unused, GError **error);
gboolean anto_display_mutation_enable(DisplayContext *context, json_object *monitors,
                                const char *name, const char *unused,
                                GError **error);
gboolean anto_display_mutation_disable(DisplayContext *context, json_object *monitors,
                                 const char *name, const char *unused,
                                 GError **error);
gboolean anto_display_mutation_toggle(DisplayContext *context, json_object *monitors,
                                const char *name, const char *unused,
                                GError **error);
gboolean anto_display_mutation_only(DisplayContext *context, json_object *monitors,
                              const char *name, const char *unused,
                              GError **error);
gboolean anto_display_mutation_extend(DisplayContext *context, json_object *monitors,
                                const char *direction, const char *unused,
                                GError **error);
gboolean anto_display_mutation_mirror(DisplayContext *context, json_object *monitors,
                                const char *requested, const char *unused,
                                GError **error);
gboolean anto_display_mutation_scale(DisplayContext *context, json_object *monitors,
                               const char *name, const char *scale,
                               GError **error);
gboolean anto_display_mutation_transform(DisplayContext *context,
                                   json_object *monitors, const char *name,
                                   const char *transform_text, GError **error);
gboolean anto_display_mutation_mode(DisplayContext *context, json_object *monitors,
                              const char *name, const char *requested,
                              GError **error);
gboolean anto_display_mutation_position(DisplayContext *context,
                                  json_object *monitors, const char *name,
                                  const char *direction, GError **error);
