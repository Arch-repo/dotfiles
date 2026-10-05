#pragma once
/* Page-private state and cross-file contract. Public API stays in menu.h. */
#include "menu.h"

#include <string.h>

#define EMOJI_GRID_COLUMNS 8
#define EMOJI_PAGE_ROWS 6

typedef struct _EmojiRecord EmojiRecord;
typedef struct _EmojiRecordClass EmojiRecordClass;

struct _EmojiRecord {
    GObject parent_instance;
    char *symbol;
    char *group;
    char *subgroup;
    char *name;
    char *keywords;
    char *search;
};

struct _EmojiRecordClass {
    GObjectClass parent_class;
};



typedef struct {
    MenuApp *app;
    GtkCustomFilter *filter;
    GtkFilterListModel *filtered;
    GtkSingleSelection *selection;
    GtkWidget *view;
    GtkEventController *key_controller;
    char **query_tokens;
    guint total;
} EmojiPicker;

typedef struct {
    const char *symbol;
    const char *name;
    const char *keywords;
} FallbackEmoji;

extern GListStore *anto_emoji_catalog;
extern gboolean anto_emoji_catalog_is_fallback;
extern const FallbackEmoji anto_emoji_fallback_emojis[48];

const char *anto_emoji_localized_group(const char *group);
const char *anto_emoji_localized_search_terms(const char *group);
gboolean anto_emoji_contains_ascii_term(const char *haystack, const char *term);
char *anto_emoji_localized_item_terms(const char *name, const char *keywords);
void anto_emoji_record_finalize(GObject *object);
EmojiRecord *anto_emoji_record_new(const char *symbol, const char *group,
                                     const char *subgroup, const char *name,
                                     const char *keywords);
guint anto_emoji_load_system_catalog(GListStore *store);
void anto_emoji_load_fallback_catalog(GListStore *store);
GListStore *anto_emoji_get_catalog(void);
gboolean anto_emoji_matches(gpointer item, gpointer data);
void anto_emoji_factory_setup(GtkSignalListItemFactory *factory, GtkListItem *item,
                          gpointer data);
void anto_emoji_factory_bind(GtkSignalListItemFactory *factory, GtkListItem *item,
                         gpointer data);
GtkWidget *anto_emoji_create_grid(EmojiPicker *picker);
guint anto_emoji_columns(EmojiPicker *picker);
void anto_emoji_copy_record(EmojiPicker *picker, guint position);
void anto_emoji_activated(GtkGridView *view, guint position, gpointer data);
gboolean anto_emoji_key_pressed(GtkEventControllerKey *controller, guint keyval,
                                  guint keycode, GdkModifierType state,
                                  gpointer data);
void anto_emoji_update_footer(EmojiPicker *picker, const char *query);
void anto_emoji_search_changed(MenuApp *app, const char *query, gpointer data);
void anto_emoji_picker_free(gpointer data);
void anto_emoji_open_characters(GtkButton *button, gpointer data);
void menu_show_emoji(MenuApp *app);
GType emoji_record_get_type(void);
