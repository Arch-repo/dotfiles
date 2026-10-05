#pragma once
/* Page-private state and cross-file contract. Public API stays in menu.h. */
#include "menu.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define AUDIO_RUNTIME_KEY "anto-menu-audio-runtime"

typedef struct {
    double sink_volume;
    double source_volume;
    gboolean sink_muted;
    gboolean source_muted;
    gboolean parsed;
    char *sink_name;
    char *player;
    char *status;
    char *artist;
    char *title;
    char *album;
} AudioSnapshot;

typedef struct {
    MenuApp *app;
    AntoQuery *query;
    AudioSnapshot *snapshot;
    GWeakRef root;
    gboolean refresh_pending;
    gboolean interacting;
    gboolean applying;
    gboolean full_view;
    guint interaction_source;
    GtkWidget *sink_icon;
    GtkWidget *source_icon;
    GtkWidget *sink_detail;
    GtkWidget *source_detail;
    GtkWidget *sink_scale;
    GtkWidget *source_scale;
    GtkWidget *player_detail;
    GtkWidget *player_row;
} AudioRuntime;


typedef struct {
    AudioRuntime *runtime;
    const char *target;
} AudioScaleBinding;



void anto_audio_snapshot_free(AudioSnapshot *snapshot);
void anto_audio_runtime_free(gpointer data);
AudioRuntime *anto_audio_runtime_get(MenuApp *app);
gboolean anto_audio_root_is_current(AudioRuntime *runtime);
double anto_audio_parse_volume(const char *text, gboolean *muted,
                           gboolean *valid);
AudioSnapshot *anto_audio_snapshot_parse(const char *output);
char *anto_audio_player_text(const AudioSnapshot *snapshot);
GtkWidget *anto_audio_find_css_descendant(GtkWidget *widget,
                                      const char *css_class);
gboolean anto_audio_player_search_set(AudioRuntime *runtime,
                                        const char *detail);
gboolean anto_audio_interaction_finished(gpointer data);
void anto_audio_scale_changed(GtkRange *range, gpointer data);
void anto_audio_scale_binding_free(gpointer data, GClosure *closure);
GtkWidget *anto_audio_scale_row(AudioRuntime *runtime, const char *icon_name,
                                  const char *title, double maximum,
                                  GtkWidget **icon_out,
                                  GtkWidget **detail_out,
                                  GtkWidget **scale_out,
                                  const char *target);
void anto_audio_apply_snapshot(AudioRuntime *runtime,
                                 const AudioSnapshot *snapshot);
void anto_audio_render(AudioRuntime *runtime,
                         const AudioSnapshot *snapshot);
void anto_audio_start_snapshot(MenuApp *app);
void menu_audio_live_event(MenuApp *app);
void menu_show_audio(MenuApp *app);
