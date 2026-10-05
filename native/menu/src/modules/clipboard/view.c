#include "internal.h"

ClipboardView *anto_clipboard_current_view(MenuApp *app) {
    if (!app || !app->window ||
        g_strcmp0(app->current_page, "clipboard") != 0)
        return NULL;
    return g_object_get_data(G_OBJECT(app->window), CLIPBOARD_VIEW_KEY);
}

void anto_clipboard_label_set(GtkWidget *label, const char *text) {
    if (!GTK_IS_LABEL(label)) return;
    const char *current = gtk_label_get_text(GTK_LABEL(label));
    if (g_strcmp0(current, text ? text : "") != 0)
        gtk_label_set_text(GTK_LABEL(label), text ? text : "");
}

void anto_clipboard_search_set(ClipboardRow *row,
                                 const ClipboardRecord *record) {
    g_autofree char *combined = g_strdup_printf(
        "%s %s %s", record->title, record->subtitle, record->record);
    g_autofree char *search = g_utf8_strdown(combined, -1);
    const char *current =
        g_object_get_data(G_OBJECT(row->row), "menu-search");
    if (g_strcmp0(current, search) != 0)
        g_object_set_data_full(G_OBJECT(row->row), "menu-search",
                               g_strdup(search), g_free);
}

void anto_clipboard_row_update(ClipboardRow *row,
                                 const ClipboardRecord *record,
                                 gboolean newest) {
    anto_clipboard_label_set(row->title, record->title);
    anto_clipboard_label_set(row->subtitle, record->subtitle);
    anto_clipboard_label_set(row->badge, "ULTIMO");
    gtk_widget_set_visible(row->badge, newest);
    if (row->image != record->image) {
        gtk_image_set_from_icon_name(
            GTK_IMAGE(row->icon),
            record->image ? "image-x-generic-symbolic"
                          : "edit-copy-symbolic");
        row->image = record->image;
    }
    if (g_strcmp0(row->entry->id, record->key) != 0) {
        g_free(row->entry->id);
        row->entry->id = g_strdup(record->key);
    }
    anto_clipboard_search_set(row, record);
}

ClipboardRow *anto_clipboard_row_add(ClipboardView *view,
                                       const ClipboardRecord *record,
                                       gboolean newest) {
    GtkWidget *before = gtk_widget_get_last_child(view->app->list);
    ClipboardEntry *entry = g_new0(ClipboardEntry, 1);
    /* ClipHist accepts its numeric key directly.  Feeding the complete
     * human-readable list line made decoding depend on a mutable/truncated
     * preview and was the source of intermittent "Elemento non disponibile"
     * failures. */
    entry->id = g_strdup(record->key);
    menu_add_item(view->app,
                  record->image ? "image-x-generic-symbolic"
                                : "edit-copy-symbolic",
                  record->title, record->subtitle, "ULTIMO",
                  anto_clipboard_paste_record, entry, anto_clipboard_entry_free);
    GtkWidget *row = gtk_widget_get_last_child(view->app->list);
    if (!row || row == before) return NULL;

    ClipboardRow *item = g_new0(ClipboardRow, 1);
    item->key = g_strdup(record->key);
    item->entry = entry;
    item->row = row;
    item->icon = anto_clipboard_find(row, NULL, TRUE);
    item->title = anto_clipboard_find(row, "item-title", FALSE);
    item->subtitle = anto_clipboard_find(row, "item-subtitle", FALSE);
    item->badge = anto_clipboard_find(row, "item-badge", FALSE);
    item->image = record->image;
    if (!item->icon || !item->title || !item->subtitle || !item->badge) {
        gtk_list_box_remove(GTK_LIST_BOX(view->app->list), row);
        anto_clipboard_row_free(item);
        return NULL;
    }
    anto_clipboard_row_update(item, record, newest);
    return item;
}

gboolean anto_clipboard_restore_scroll(gpointer data) {
    ClipboardView *view = data;
    view->scroll_restore_source = 0;
    if (!view->app || !view->app->window ||
        g_strcmp0(view->app->current_page, "clipboard") != 0 ||
        g_object_get_data(G_OBJECT(view->app->window),
                          CLIPBOARD_VIEW_KEY) != view)
        return G_SOURCE_REMOVE;
    GtkAdjustment *adjustment = gtk_scrolled_window_get_vadjustment(
        GTK_SCROLLED_WINDOW(view->app->list_scroll));
    double lower = gtk_adjustment_get_lower(adjustment);
    double maximum =
        MAX(lower, gtk_adjustment_get_upper(adjustment) -
                       gtk_adjustment_get_page_size(adjustment));
    gtk_adjustment_set_value(
        adjustment, CLAMP(view->scroll_value, lower, maximum));
    return G_SOURCE_REMOVE;
}

void anto_clipboard_schedule_scroll_restore(ClipboardView *view,
                                              double value) {
    view->scroll_value = value;
    if (view->scroll_restore_source) return;
    view->scroll_restore_source = g_idle_add_full(
        G_PRIORITY_DEFAULT_IDLE, anto_clipboard_restore_scroll, view, NULL);
}

void anto_clipboard_reconcile(ClipboardView *view,
                                const ClipboardSnapshot *snapshot) {
    MenuApp *app = view->app;
    GtkAdjustment *adjustment = gtk_scrolled_window_get_vadjustment(
        GTK_SCROLLED_WINDOW(app->list_scroll));
    double scroll = gtk_adjustment_get_value(adjustment);
    GtkListBoxRow *selected =
        gtk_list_box_get_selected_row(GTK_LIST_BOX(app->list));
    if (selected) g_object_ref(selected);
    GtkWidget *focus = gtk_window_get_focus(app->window);
    if (focus) g_object_ref(focus);

    g_autoptr(GHashTable) present =
        g_hash_table_new(g_str_hash, g_str_equal);
    for (guint i = 0; i < snapshot->records->len; i++) {
        ClipboardRecord *record =
            g_ptr_array_index(snapshot->records, i);
        g_hash_table_add(present, record->key);
    }

    gboolean structure_changed = FALSE;
    GHashTableIter iterator;
    gpointer key = NULL;
    gpointer value = NULL;
    g_hash_table_iter_init(&iterator, view->rows);
    while (g_hash_table_iter_next(&iterator, &key, &value)) {
        if (g_hash_table_contains(present, key)) continue;
        ClipboardRow *row = value;
        if (row->row &&
            gtk_widget_get_parent(row->row) == app->list)
            gtk_list_box_remove(GTK_LIST_BOX(app->list), row->row);
        g_hash_table_iter_remove(&iterator);
        structure_changed = TRUE;
    }

    for (guint i = 0; i < snapshot->records->len; i++) {
        ClipboardRecord *record =
            g_ptr_array_index(snapshot->records, i);
        ClipboardRow *row =
            g_hash_table_lookup(view->rows, record->key);
        if (!row) {
            row = anto_clipboard_row_add(view, record, i == 0);
            if (!row) continue;
            g_hash_table_insert(view->rows, g_strdup(record->key), row);
            structure_changed = TRUE;
        } else {
            anto_clipboard_row_update(row, record, i == 0);
        }

        int desired = (int)i + 1;
        int current =
            gtk_list_box_row_get_index(GTK_LIST_BOX_ROW(row->row));
        if (current != desired) {
            g_object_ref(row->row);
            gtk_list_box_remove(GTK_LIST_BOX(app->list), row->row);
            gtk_list_box_insert(GTK_LIST_BOX(app->list), row->row,
                                desired);
            g_object_unref(row->row);
            structure_changed = TRUE;
        }
    }

    gboolean empty = snapshot->records->len == 0;
    anto_clipboard_label_set(view->empty_title,
                        empty ? "La cronologia è vuota"
                              : "Caricamento cronologia…");
    anto_clipboard_label_set(
        view->empty_subtitle,
        empty ? "Copia del testo o un’immagine per iniziare"
              : "Lettura della cronologia locale ClipHist");
    gtk_widget_set_visible(view->empty_row, empty);

    g_autofree char *subtitle = g_strdup_printf(
        "%u elementi · cronologia locale ClipHist",
        snapshot->total_count);
    anto_clipboard_label_set(app->page_subtitle, subtitle);
    gtk_list_box_invalidate_filter(GTK_LIST_BOX(app->list));

    if (selected &&
        gtk_widget_get_parent(GTK_WIDGET(selected)) == app->list)
        gtk_list_box_select_row(GTK_LIST_BOX(app->list), selected);
    if (focus && gtk_window_get_focus(app->window) != focus &&
        gtk_widget_get_root(focus) == GTK_ROOT(app->window))
        gtk_widget_grab_focus(focus);
    g_clear_object(&selected);
    g_clear_object(&focus);

    if (structure_changed)
        anto_clipboard_schedule_scroll_restore(view, scroll);
}

void menu_show_clipboard(MenuApp *app) {
    g_object_set_data_full(G_OBJECT(app->window), CLIPBOARD_VIEW_KEY,
                           NULL, NULL);
    menu_page_begin(app, "edit-paste-symbolic", "Cronologia appunti",
                    "Lettura della cronologia locale ClipHist",
                    "Cerca nel testo copiato…");

    ClipboardView *view = g_new0(ClipboardView, 1);
    view->app = app;
    view->rows = g_hash_table_new_full(
        g_str_hash, g_str_equal, g_free, anto_clipboard_row_free);
    g_object_set_data_full(G_OBJECT(app->window), CLIPBOARD_VIEW_KEY,
                           view, anto_clipboard_view_free);

    menu_add_item(app, "edit-paste-symbolic",
                  "Caricamento cronologia…",
                  "Lettura della cronologia locale ClipHist",
                  NULL, NULL, NULL, NULL);
    view->empty_row = gtk_widget_get_last_child(app->list);
    view->empty_title =
        anto_clipboard_find(view->empty_row, "item-title", FALSE);
    view->empty_subtitle =
        anto_clipboard_find(view->empty_row, "item-subtitle", FALSE);
    g_object_set_data_full(G_OBJECT(view->empty_row), "menu-search",
                           NULL, NULL);

    menu_set_footer(
        app, "↑↓ naviga  ·  Invio copia  ·  aggiornamento automatico");
    anto_clipboard_snapshot_start(view);
}
