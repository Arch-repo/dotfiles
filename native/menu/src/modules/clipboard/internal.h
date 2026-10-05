#pragma once
/* Page-private state and cross-file contract. Public API stays in menu.h. */
#include "menu.h"

#include <string.h>

#define CLIPBOARD_VIEW_KEY "anto-menu-clipboard-view"
#define CLIPBOARD_MAX_ROWS 180u
#define CLIPBOARD_FAILURE_THROTTLE_USEC (8 * G_USEC_PER_SEC)

typedef struct {
    char *id;
} ClipboardEntry;

typedef struct {
    char *key;
    char *record;
    char *title;
    char *subtitle;
    gboolean image;
} ClipboardRecord;

typedef struct {
    GPtrArray *records;
    guint total_count;
} ClipboardSnapshot;

typedef struct {
    char *key;
    ClipboardEntry *entry;
    GtkWidget *row;
    GtkWidget *icon;
    GtkWidget *title;
    GtkWidget *subtitle;
    GtkWidget *badge;
    gboolean image;
} ClipboardRow;

typedef struct {
    MenuApp *app;
    GHashTable *rows;
    GtkWidget *empty_row;
    GtkWidget *empty_title;
    GtkWidget *empty_subtitle;
    GSubprocess *process;
    GCancellable *cancellable;
    gboolean refresh_pending;
    guint debounce_source;
    guint scroll_restore_source;
    double scroll_value;
    gboolean paste_in_flight;
    gint64 last_failure_time;
    char *last_failure_record;
} ClipboardView;

typedef struct {
    GWeakRef window;
    GSubprocess *process;
} ClipboardPending;

typedef struct {
    MenuApp *app;
    GWeakRef window;
    GSubprocess *process;
    GCancellable *cancellable;
    char *record_id;
} ClipboardPaste;



void anto_clipboard_entry_free(gpointer data);
void anto_clipboard_record_free(gpointer data);
void anto_clipboard_snapshot_free(ClipboardSnapshot *snapshot);
void anto_clipboard_row_free(gpointer data);
void anto_clipboard_pending_free(ClipboardPending *pending);
void anto_clipboard_paste_free(ClipboardPaste *paste);
void anto_clipboard_view_free(gpointer data);
ClipboardView *anto_clipboard_current_view(MenuApp *app);
void anto_clipboard_failure(ClipboardView *view, const char *record,
                              const char *message);
void anto_clipboard_paste_copied(GObject *object, GAsyncResult *result,
                                   gpointer data);
void anto_clipboard_paste_decoded(GObject *object, GAsyncResult *result,
                                    gpointer data);
void anto_clipboard_paste_record(MenuApp *app, gpointer data);
char *anto_clipboard_record_id(const char *line, gsize length);
char *anto_clipboard_preview(const char *preview);
ClipboardSnapshot *anto_clipboard_snapshot_parse(const char *output);
void anto_clipboard_label_set(GtkWidget *label, const char *text);
GtkWidget *anto_clipboard_find(GtkWidget *root, const char *css_class,
                                 gboolean image);
void anto_clipboard_search_set(ClipboardRow *row,
                                 const ClipboardRecord *record);
void anto_clipboard_row_update(ClipboardRow *row,
                                 const ClipboardRecord *record,
                                 gboolean newest);
ClipboardRow *anto_clipboard_row_add(ClipboardView *view,
                                       const ClipboardRecord *record,
                                       gboolean newest);
gboolean anto_clipboard_restore_scroll(gpointer data);
void anto_clipboard_schedule_scroll_restore(ClipboardView *view,
                                              double value);
void anto_clipboard_reconcile(ClipboardView *view,
                                const ClipboardSnapshot *snapshot);
void anto_clipboard_snapshot_finished(GObject *object,
                                        GAsyncResult *result,
                                        gpointer data);
void anto_clipboard_snapshot_start(ClipboardView *view);
gboolean anto_clipboard_debounced_refresh(gpointer data);
void menu_clipboard_live_event(MenuApp *app);
void menu_show_clipboard(MenuApp *app);
