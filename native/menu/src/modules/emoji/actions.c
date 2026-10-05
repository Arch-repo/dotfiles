#include "internal.h"

void anto_emoji_activated(GtkGridView *view, guint position, gpointer data) {
    (void)view;
    anto_emoji_copy_record(data, position);
}

gboolean anto_emoji_key_pressed(GtkEventControllerKey *controller, guint keyval,
                                  guint keycode, GdkModifierType state,
                                  gpointer data) {
    (void)controller;
    (void)keycode;
    (void)state;
    EmojiPicker *picker = data;
    guint count = g_list_model_get_n_items(G_LIST_MODEL(picker->filtered));
    if (!count) return FALSE;
    guint current = gtk_single_selection_get_selected(picker->selection);
    if (current == GTK_INVALID_LIST_POSITION) current = 0;
    guint columns = anto_emoji_columns(picker);
    gint64 next = current;
    if (keyval == GDK_KEY_Left) next--;
    else if (keyval == GDK_KEY_Right) next++;
    else if (keyval == GDK_KEY_Up) next -= columns;
    else if (keyval == GDK_KEY_Down) next += columns;
    else if (keyval == GDK_KEY_Page_Up)
        next -= columns * EMOJI_PAGE_ROWS;
    else if (keyval == GDK_KEY_Page_Down)
        next += columns * EMOJI_PAGE_ROWS;
    else if (keyval == GDK_KEY_Home) next = 0;
    else if (keyval == GDK_KEY_End) next = count - 1;
    else if (keyval == GDK_KEY_Return || keyval == GDK_KEY_KP_Enter) {
        anto_emoji_copy_record(picker, current);
        return TRUE;
    } else {
        return FALSE;
    }
    next = CLAMP(next, 0, (gint64)count - 1);
    gtk_grid_view_scroll_to(GTK_GRID_VIEW(picker->view), (guint)next,
                            GTK_LIST_SCROLL_FOCUS | GTK_LIST_SCROLL_SELECT, NULL);
    return TRUE;
}

void anto_emoji_search_changed(MenuApp *app, const char *query, gpointer data) {
    (void)app;
    EmojiPicker *picker = data;
    g_strfreev(picker->query_tokens);
    picker->query_tokens = NULL;
    if (query && *query) {
        g_autofree char *folded = g_utf8_casefold(query, -1);
        picker->query_tokens = g_strsplit_set(folded, " \t\r\n", -1);
    }
    gtk_filter_changed(GTK_FILTER(picker->filter), GTK_FILTER_CHANGE_DIFFERENT);
    guint count = g_list_model_get_n_items(G_LIST_MODEL(picker->filtered));
    gtk_single_selection_set_selected(
        picker->selection, count ? 0 : GTK_INVALID_LIST_POSITION);
    if (count)
        gtk_grid_view_scroll_to(GTK_GRID_VIEW(picker->view), 0,
                                GTK_LIST_SCROLL_SELECT, NULL);
    anto_emoji_update_footer(picker, query);
}
