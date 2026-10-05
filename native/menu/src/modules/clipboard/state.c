#include "internal.h"

void anto_clipboard_entry_free(gpointer data) {
    ClipboardEntry *entry = data;
    if (!entry) return;
    g_free(entry->id);
    g_free(entry);
}

void anto_clipboard_record_free(gpointer data) {
    ClipboardRecord *record = data;
    if (!record) return;
    g_free(record->key);
    g_free(record->record);
    g_free(record->title);
    g_free(record->subtitle);
    g_free(record);
}

void anto_clipboard_snapshot_free(ClipboardSnapshot *snapshot) {
    if (!snapshot) return;
    g_clear_pointer(&snapshot->records, g_ptr_array_unref);
    g_free(snapshot);
}

void anto_clipboard_row_free(gpointer data) {
    ClipboardRow *row = data;
    if (!row) return;
    g_free(row->key);
    g_free(row);
}

void anto_clipboard_pending_free(ClipboardPending *pending) {
    if (!pending) return;
    g_weak_ref_clear(&pending->window);
    g_clear_object(&pending->process);
    g_free(pending);
}

void anto_clipboard_paste_free(ClipboardPaste *paste) {
    if (!paste) return;
    if (paste->cancellable)
        g_cancellable_cancel(paste->cancellable);
    if (paste->process)
        g_subprocess_force_exit(paste->process);
    g_weak_ref_clear(&paste->window);
    g_clear_object(&paste->process);
    g_clear_object(&paste->cancellable);
    g_free(paste->record_id);
    g_free(paste);
}

void anto_clipboard_view_free(gpointer data) {
    ClipboardView *view = data;
    if (!view) return;
    if (view->debounce_source)
        g_source_remove(view->debounce_source);
    if (view->scroll_restore_source)
        g_source_remove(view->scroll_restore_source);
    if (view->cancellable)
        g_cancellable_cancel(view->cancellable);
    if (view->process)
        g_subprocess_force_exit(view->process);
    g_clear_object(&view->process);
    g_clear_object(&view->cancellable);
    g_clear_pointer(&view->rows, g_hash_table_unref);
    g_clear_pointer(&view->last_failure_record, g_free);
    g_free(view);
}

void anto_clipboard_failure(ClipboardView *view, const char *record,
                              const char *message) {
    if (!view) return;
    view->paste_in_flight = FALSE;

    const gint64 now = g_get_monotonic_time();
    const gboolean duplicate =
        g_strcmp0(view->last_failure_record, record) == 0 &&
        view->last_failure_time > 0 &&
        now - view->last_failure_time < CLIPBOARD_FAILURE_THROTTLE_USEC;
    if (!duplicate) {
        g_free(view->last_failure_record);
        view->last_failure_record = g_strdup(record);
        view->last_failure_time = now;
        menu_notify("Appunti", message);
    }

    /* A decode failure normally means that the row became stale between the
     * ClipHist snapshot and activation.  Reconcile the list immediately so
     * the unavailable row cannot be selected over and over. */
    anto_clipboard_snapshot_start(view);
}

void anto_clipboard_paste_copied(GObject *object, GAsyncResult *result,
                                   gpointer data) {
    ClipboardPaste *paste = data;
    g_autoptr(GError) error = NULL;
    gboolean copied = g_subprocess_communicate_finish(
        G_SUBPROCESS(object), result, NULL, NULL, &error);
    copied = copied && g_subprocess_get_successful(G_SUBPROCESS(object));
    g_clear_object(&paste->cancellable);

    GtkWidget *window = g_weak_ref_get(&paste->window);
    ClipboardView *view = window ? anto_clipboard_current_view(paste->app) : NULL;
    if (view) view->paste_in_flight = FALSE;
    if (window && copied) {
        menu_close(paste->app);
    } else if (window && !copied && view) {
        anto_clipboard_failure(view, paste->record_id,
                          "Impossibile copiare l’elemento");
    }
    g_clear_object(&window);
    anto_clipboard_paste_free(paste);
}

void anto_clipboard_paste_decoded(GObject *object, GAsyncResult *result,
                                    gpointer data) {
    ClipboardPaste *paste = data;
    GBytes *decoded_raw = NULL;
    g_autoptr(GError) error = NULL;
    gboolean decoded = g_subprocess_communicate_finish(
        G_SUBPROCESS(object), result, NULL, &decoded_raw, &error);
    g_autoptr(GBytes) bytes = decoded_raw;
    decoded = decoded &&
              g_subprocess_get_successful(G_SUBPROCESS(object)) && bytes;
    g_clear_object(&paste->cancellable);

    GtkWidget *window = g_weak_ref_get(&paste->window);
    ClipboardView *view = window ? anto_clipboard_current_view(paste->app) : NULL;
    if (!window || !decoded) {
        if (view)
            anto_clipboard_failure(view, paste->record_id,
                              "Elemento non disponibile");
        g_clear_object(&window);
        anto_clipboard_paste_free(paste);
        return;
    }
    g_clear_object(&window);

    const char *copy_argv[] = {
        "/usr/bin/timeout", "--foreground", "--kill-after=1", "5",
        "wl-copy", NULL
    };
    g_autoptr(GError) spawn_error = NULL;
    GSubprocess *copy = g_subprocess_newv(
        copy_argv, G_SUBPROCESS_FLAGS_STDIN_PIPE |
                       G_SUBPROCESS_FLAGS_STDERR_SILENCE,
        &spawn_error);
    if (!copy) {
        view = anto_clipboard_current_view(paste->app);
        if (view)
            anto_clipboard_failure(view, paste->record_id,
                              "Impossibile avviare wl-copy");
        anto_clipboard_paste_free(paste);
        return;
    }
    g_set_object(&paste->process, copy);
    g_object_unref(copy);
    paste->cancellable = g_cancellable_new();
    g_subprocess_communicate_async(
        paste->process, bytes, paste->cancellable,
        anto_clipboard_paste_copied, paste);
}

void anto_clipboard_paste_record(MenuApp *app, gpointer data) {
    ClipboardEntry *entry = data;
    if (!entry || !entry->id) return;

    ClipboardView *view = anto_clipboard_current_view(app);
    if (!view || view->paste_in_flight) return;
    const gint64 now = g_get_monotonic_time();
    if (g_strcmp0(view->last_failure_record, entry->id) == 0 &&
        view->last_failure_time > 0 &&
        now - view->last_failure_time < CLIPBOARD_FAILURE_THROTTLE_USEC)
        return;
    view->paste_in_flight = TRUE;

    const char *decode_argv[] = {
        "/usr/bin/timeout", "--foreground", "--kill-after=1", "5",
        "cliphist", "decode", NULL
    };
    g_autoptr(GError) error = NULL;
    GSubprocess *decode = g_subprocess_newv(
        decode_argv, G_SUBPROCESS_FLAGS_STDIN_PIPE |
                         G_SUBPROCESS_FLAGS_STDOUT_PIPE |
                         G_SUBPROCESS_FLAGS_STDERR_SILENCE,
        &error);
    if (!decode) {
        anto_clipboard_failure(view, entry->id,
                          "Impossibile decodificare l’elemento");
        return;
    }

    ClipboardPaste *paste = g_new0(ClipboardPaste, 1);
    paste->app = app;
    paste->record_id = g_strdup(entry->id);
    paste->process = g_object_ref(decode);
    paste->cancellable = g_cancellable_new();
    g_weak_ref_init(&paste->window, G_OBJECT(app->window));
    g_autoptr(GBytes) input =
        g_bytes_new(paste->record_id, strlen(paste->record_id));
    g_subprocess_communicate_async(
        decode, input, paste->cancellable,
        anto_clipboard_paste_decoded, paste);
    g_object_unref(decode);
}

char *anto_clipboard_record_id(const char *line, gsize length) {
    const char *tab = memchr(line, '\t', length);
    if (!tab || tab == line) return NULL;
    for (const char *cursor = line; cursor < tab; cursor++) {
        if (!g_ascii_isdigit(*cursor)) return NULL;
    }
    return g_strndup(line, (gsize)(tab - line));
}

char *anto_clipboard_preview(const char *preview) {
    g_autofree char *valid = g_utf8_make_valid(preview ? preview : "", -1);
    g_strdelimit(valid, "\r\n\t", ' ');
    g_strstrip(valid);
    if (!*valid) return g_strdup("Testo copiato");
    if (g_utf8_strlen(valid, -1) <= 110)
        return g_strdup(valid);
    g_autofree char *prefix = g_utf8_substring(valid, 0, 107);
    return g_strconcat(prefix, "…", NULL);
}

ClipboardSnapshot *anto_clipboard_snapshot_parse(const char *output) {
    ClipboardSnapshot *snapshot = g_new0(ClipboardSnapshot, 1);
    snapshot->records =
        g_ptr_array_new_with_free_func(anto_clipboard_record_free);
    g_autoptr(GHashTable) seen =
        g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
    g_auto(GStrv) lines = g_strsplit(output ? output : "", "\n", -1);

    for (guint i = 0; lines[i]; i++) {
        gsize length = strlen(lines[i]);
        if (!length) continue;
        snapshot->total_count++;
        if (snapshot->records->len >= CLIPBOARD_MAX_ROWS)
            continue;

        g_autofree char *key = anto_clipboard_record_id(lines[i], length);
        if (!key) continue;
        if (g_hash_table_contains(seen, key))
            continue;
        g_hash_table_add(seen, g_strdup(key));

        const char *tab = strchr(lines[i], '\t');
        const char *preview = tab ? tab + 1 : lines[i];
        ClipboardRecord *record = g_new0(ClipboardRecord, 1);
        record->key = g_steal_pointer(&key);
        record->record = g_strdup(lines[i]);
        record->image = g_str_has_prefix(preview, "[[ binary data");
        record->title = record->image
                            ? g_strdup("Immagine copiata")
                            : anto_clipboard_preview(preview);
        record->subtitle = record->image
                               ? anto_clipboard_preview(preview)
                               : g_strdup("Seleziona per copiarlo di nuovo");
        g_ptr_array_add(snapshot->records, record);
    }
    return snapshot;
}

GtkWidget *anto_clipboard_find(GtkWidget *root, const char *css_class,
                                 gboolean image) {
    if (!root) return NULL;
    if (image && GTK_IS_IMAGE(root))
        return root;
    if (!image && GTK_IS_LABEL(root) &&
        gtk_widget_has_css_class(root, css_class))
        return root;
    for (GtkWidget *child = gtk_widget_get_first_child(root); child;
         child = gtk_widget_get_next_sibling(child)) {
        GtkWidget *match = anto_clipboard_find(child, css_class, image);
        if (match) return match;
    }
    return NULL;
}

void anto_clipboard_snapshot_finished(GObject *object,
                                        GAsyncResult *result,
                                        gpointer data) {
    ClipboardPending *pending = data;
    char *output = NULL;
    char *error_text = NULL;
    g_autoptr(GError) error = NULL;
    gboolean ok = g_subprocess_communicate_utf8_finish(
        G_SUBPROCESS(object), result, &output, &error_text, &error);
    ok = ok && g_subprocess_get_successful(G_SUBPROCESS(object));

    GtkWidget *window = g_weak_ref_get(&pending->window);
    ClipboardView *view =
        window ? g_object_get_data(G_OBJECT(window),
                                   CLIPBOARD_VIEW_KEY)
               : NULL;
    if (!view || view->process != G_SUBPROCESS(object)) {
        g_clear_object(&window);
        g_free(output);
        g_free(error_text);
        anto_clipboard_pending_free(pending);
        return;
    }

    g_clear_object(&view->process);
    g_clear_object(&view->cancellable);
    gboolean on_page =
        g_strcmp0(view->app->current_page, "clipboard") == 0;
    if (ok && on_page) {
        ClipboardSnapshot *snapshot = anto_clipboard_snapshot_parse(output);
        anto_clipboard_reconcile(view, snapshot);
        anto_clipboard_snapshot_free(snapshot);
    } else if (on_page && g_hash_table_size(view->rows) == 0) {
        anto_clipboard_label_set(view->app->page_subtitle,
                            "Cronologia locale non disponibile");
        anto_clipboard_label_set(view->empty_title,
                            "ClipHist non è disponibile");
        anto_clipboard_label_set(
            view->empty_subtitle,
            "Controlla il servizio della cronologia appunti");
        gtk_widget_set_visible(view->empty_row, TRUE);
    }

    gboolean again = view->refresh_pending;
    view->refresh_pending = FALSE;
    if (again && on_page)
        anto_clipboard_snapshot_start(view);

    g_clear_object(&window);
    g_free(output);
    g_free(error_text);
    anto_clipboard_pending_free(pending);
}

void anto_clipboard_snapshot_start(ClipboardView *view) {
    if (!view || !view->app ||
        g_strcmp0(view->app->current_page, "clipboard") != 0)
        return;
    if (view->process) {
        view->refresh_pending = TRUE;
        return;
    }

    const char *argv[] = {
        "/usr/bin/timeout", "--foreground", "--kill-after=1", "3",
        "cliphist", "list", NULL,
    };
    g_autoptr(GError) error = NULL;
    view->process = g_subprocess_newv(
        argv, G_SUBPROCESS_FLAGS_STDOUT_PIPE |
                  G_SUBPROCESS_FLAGS_STDERR_PIPE,
        &error);
    if (!view->process) {
        anto_clipboard_label_set(view->app->page_subtitle,
                            "Cronologia locale non disponibile");
        anto_clipboard_label_set(view->empty_title,
                            "ClipHist non è disponibile");
        anto_clipboard_label_set(
            view->empty_subtitle,
            "Installa o riavvia il servizio della cronologia");
        gtk_widget_set_visible(view->empty_row, TRUE);
        return;
    }

    view->cancellable = g_cancellable_new();
    ClipboardPending *pending = g_new0(ClipboardPending, 1);
    pending->process = g_object_ref(view->process);
    g_weak_ref_init(&pending->window, G_OBJECT(view->app->window));
    g_subprocess_communicate_utf8_async(
        view->process, NULL, view->cancellable,
        anto_clipboard_snapshot_finished, pending);
}

gboolean anto_clipboard_debounced_refresh(gpointer data) {
    ClipboardView *view = data;
    view->debounce_source = 0;
    if (view->app && view->app->window &&
        g_strcmp0(view->app->current_page, "clipboard") == 0 &&
        g_object_get_data(G_OBJECT(view->app->window),
                          CLIPBOARD_VIEW_KEY) == view)
        anto_clipboard_snapshot_start(view);
    return G_SOURCE_REMOVE;
}

void menu_clipboard_live_event(MenuApp *app) {
    if (!app || !app->window ||
        g_strcmp0(app->current_page, "clipboard") != 0)
        return;
    ClipboardView *view =
        g_object_get_data(G_OBJECT(app->window), CLIPBOARD_VIEW_KEY);
    if (!view) return;
    if (view->debounce_source)
        g_source_remove(view->debounce_source);
    view->debounce_source =
        g_timeout_add(180, anto_clipboard_debounced_refresh, view);
}
