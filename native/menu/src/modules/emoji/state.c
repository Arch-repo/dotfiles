#include "internal.h"

G_DEFINE_FINAL_TYPE(EmojiRecord, emoji_record, G_TYPE_OBJECT)
GListStore *anto_emoji_catalog;
gboolean anto_emoji_catalog_is_fallback;
const FallbackEmoji anto_emoji_fallback_emojis[] = {
    {"😀", "Sorriso", "felice faccia smile"},
    {"😃", "Sorriso grande", "felice occhi smile"},
    {"😂", "Risate", "lacrime ridere lol"},
    {"🥹", "Commosso", "emozione lacrima"},
    {"😍", "Innamorato", "amore occhi cuore"},
    {"😎", "Fantastico", "cool occhiali"},
    {"🤔", "Pensando", "dubbio idea"},
    {"🫡", "Saluto", "rispetto ok"},
    {"🤯", "Mente esplosa", "wow assurdo"},
    {"😴", "Sonno", "dormire stanco"},
    {"😡", "Arrabbiato", "rabbia"},
    {"👍", "Pollice su", "ok approvato sì mano"},
    {"👍🏻", "Pollice su · pelle chiara", "ok mano variante"},
    {"👍🏼", "Pollice su · pelle medio-chiara", "ok mano variante"},
    {"👍🏽", "Pollice su · pelle media", "ok mano variante"},
    {"👍🏾", "Pollice su · pelle medio-scura", "ok mano variante"},
    {"👍🏿", "Pollice su · pelle scura", "ok mano variante"},
    {"👎", "Pollice giù", "no rifiuto mano"},
    {"👏", "Applauso", "bravo complimenti mani"},
    {"🙏", "Grazie", "prego mani"},
    {"💪", "Forza", "muscoli energia corpo"},
    {"🤝", "Accordo", "stretta mano"},
    {"❤️", "Cuore rosso", "amore"},
    {"🧡", "Cuore arancione", "amore"},
    {"💛", "Cuore giallo", "amore"},
    {"💚", "Cuore verde", "amore"},
    {"💙", "Cuore blu", "amore"},
    {"💜", "Cuore viola", "amore"},
    {"🔥", "Fuoco", "hot forte"},
    {"✨", "Scintille", "magia bello"},
    {"🎉", "Festa", "party celebrazione"},
    {"🚀", "Razzo", "lancio veloce"},
    {"💡", "Idea", "lampadina pensiero"},
    {"⚡", "Fulmine", "energia rapido"},
    {"✅", "Completato", "check sì fatto"},
    {"❌", "Errore", "no croce"},
    {"⚠️", "Attenzione", "warning pericolo"},
    {"ℹ️", "Informazione", "info"},
    {"🐧", "Pinguino", "linux tux animale"},
    {"🐈", "Gatto", "animale natura"},
    {"🌱", "Germoglio", "pianta natura verde"},
    {"🍕", "Pizza", "cibo mangiare"},
    {"☕", "Caffè", "bevanda tazza"},
    {"💻", "Computer", "pc laptop codice"},
    {"🛠️", "Strumenti", "tools lavoro"},
    {"🎵", "Musica", "audio nota"},
    {"🌍", "Globo", "mondo terra viaggio"},
    {"🏠", "Casa", "edificio luogo"},
};

const char *anto_emoji_localized_group(const char *group) {
    if (g_strcmp0(group, "Smileys & Emotion") == 0) return "Faccine ed emozioni";
    if (g_strcmp0(group, "People & Body") == 0) return "Persone e corpo";
    if (g_strcmp0(group, "Animals & Nature") == 0) return "Animali e natura";
    if (g_strcmp0(group, "Food & Drink") == 0) return "Cibo e bevande";
    if (g_strcmp0(group, "Travel & Places") == 0) return "Viaggi e luoghi";
    if (g_strcmp0(group, "Activities") == 0) return "Attività";
    if (g_strcmp0(group, "Objects") == 0) return "Oggetti";
    if (g_strcmp0(group, "Symbols") == 0) return "Simboli";
    if (g_strcmp0(group, "Flags") == 0) return "Bandiere";
    if (g_strcmp0(group, "Component") == 0) return "Varianti";
    return group && *group ? group : "Emoji";
}

gboolean anto_emoji_contains_ascii_term(const char *haystack, const char *term) {
    gsize length = strlen(term);
    const char *cursor = haystack;
    while ((cursor = strstr(cursor, term)) != NULL) {
        gboolean starts_word = cursor == haystack ||
            !g_ascii_isalnum((guchar)cursor[-1]);
        gboolean ends_word = !g_ascii_isalnum((guchar)cursor[length]);
        if (starts_word && ends_word) return TRUE;
        cursor++;
    }
    return FALSE;
}

char *anto_emoji_localized_item_terms(const char *name, const char *keywords) {
    static const struct {
        const char *english;
        const char *italian;
    } aliases[] = {
        {"smile", "sorriso sorridere"}, {"grin", "sorriso"},
        {"laugh", "ridere risata"}, {"joy", "gioia"},
        {"happy", "felice felicità"}, {"sad", "triste tristezza"},
        {"cry", "piangere pianto"}, {"tear", "lacrima lacrime"},
        {"angry", "arrabbiato rabbia"}, {"love", "amore"},
        {"heart", "cuore amore"}, {"kiss", "bacio"},
        {"hand", "mano mani"}, {"thumb", "pollice"},
        {"skin tone", "tonalità pelle carnagione"}, {"hair", "capelli"},
        {"family", "famiglia"}, {"baby", "bambino neonato"},
        {"woman", "donna"}, {"man", "uomo"}, {"person", "persona"},
        {"cat", "gatto"}, {"dog", "cane"}, {"bird", "uccello"},
        {"flower", "fiore"}, {"tree", "albero"}, {"food", "cibo"},
        {"drink", "bevanda bere"}, {"coffee", "caffè"},
        {"car", "auto macchina"}, {"house", "casa"},
        {"phone", "telefono"}, {"computer", "computer pc"},
        {"music", "musica"}, {"fire", "fuoco"}, {"star", "stella"},
        {"sun", "sole"}, {"moon", "luna"}, {"flag", "bandiera"},
        {"arrow", "freccia"}, {"warning", "attenzione pericolo"},
        {"check", "spunta completato"}, {"cross", "croce errore"},
    };
    g_autofree char *combined = g_strdup_printf("%s %s", name ? name : "",
                                                keywords ? keywords : "");
    g_autofree char *folded = g_utf8_casefold(combined, -1);
    GString *terms = g_string_new(NULL);
    for (guint i = 0; i < G_N_ELEMENTS(aliases); i++) {
        if (!anto_emoji_contains_ascii_term(folded, aliases[i].english)) continue;
        if (terms->len) g_string_append_c(terms, ' ');
        g_string_append(terms, aliases[i].italian);
    }
    return g_string_free(terms, FALSE);
}

void anto_emoji_record_finalize(GObject *object) {
    EmojiRecord *record = (EmojiRecord *)object;
    g_free(record->symbol);
    g_free(record->group);
    g_free(record->subgroup);
    g_free(record->name);
    g_free(record->keywords);
    g_free(record->search);
    G_OBJECT_CLASS(emoji_record_parent_class)->finalize(object);
}

static void emoji_record_class_init(EmojiRecordClass *klass) {
    G_OBJECT_CLASS(klass)->finalize = anto_emoji_record_finalize;
}

static void emoji_record_init(EmojiRecord *record) {
    (void)record;
}

EmojiRecord *anto_emoji_record_new(const char *symbol, const char *group,
                                     const char *subgroup, const char *name,
                                     const char *keywords) {
    EmojiRecord *record = g_object_new(emoji_record_get_type(), NULL);
    record->symbol = g_strdup(symbol ? symbol : "");
    record->group = g_strdup(group ? group : "Emoji");
    record->subgroup = g_strdup(subgroup ? subgroup : "");
    record->name = g_strdup(name && *name ? name : symbol);
    record->keywords = g_strdup(keywords ? keywords : "");
    g_autofree char *item_terms = anto_emoji_localized_item_terms(record->name, record->keywords);
    g_autofree char *combined = g_strdup_printf(
        "%s %s %s %s %s %s %s %s", record->symbol, record->group,
        anto_emoji_localized_group(record->group), anto_emoji_localized_search_terms(record->group),
        record->subgroup, record->name, record->keywords, item_terms);
    record->search = g_utf8_casefold(combined, -1);
    return record;
}

guint anto_emoji_load_system_catalog(GListStore *store) {
    g_autofree char *contents = NULL;
    gsize length = 0;
    const char *path = g_getenv("ANTO_MENU_EMOJI_DATASET");
    g_autofree char *default_path = g_build_filename(g_get_user_data_dir(), "anto-desktop", "emoji.tsv", NULL);
    if (!path || !*path) path = default_path;
    if (!g_file_get_contents(path, &contents, &length, NULL) || !length)
        return 0;

    g_auto(GStrv) lines = g_strsplit(contents, "\n", -1);
    guint count = 0;
    for (guint i = 0; lines[i]; i++) {
        if (!*lines[i]) continue;
        g_auto(GStrv) fields = g_strsplit(lines[i], "\t", 5);
        if (g_strv_length(fields) < 5 || !*fields[0] || !*fields[3]) continue;
        g_strchomp(fields[4]);
        EmojiRecord *record = anto_emoji_record_new(fields[0], fields[1], fields[2],
                                                fields[3], fields[4]);
        g_list_store_append(store, record);
        g_object_unref(record);
        count++;
    }
    return count;
}

void anto_emoji_load_fallback_catalog(GListStore *store) {
    for (guint i = 0; i < G_N_ELEMENTS(anto_emoji_fallback_emojis); i++) {
        const FallbackEmoji *item = &anto_emoji_fallback_emojis[i];
        EmojiRecord *record = anto_emoji_record_new(item->symbol, "Emoji", "fallback",
                                                item->name, item->keywords);
        g_list_store_append(store, record);
        g_object_unref(record);
    }
}

GListStore *anto_emoji_get_catalog(void) {
    if (anto_emoji_catalog) return anto_emoji_catalog;
    anto_emoji_catalog = g_list_store_new(emoji_record_get_type());
    if (anto_emoji_load_system_catalog(anto_emoji_catalog) == 0) {
        anto_emoji_catalog_is_fallback = TRUE;
        anto_emoji_load_fallback_catalog(anto_emoji_catalog);
    }
    return anto_emoji_catalog;
}

gboolean anto_emoji_matches(gpointer item, gpointer data) {
    EmojiRecord *record = item;
    EmojiPicker *picker = data;
    if (!picker->query_tokens) return TRUE;
    for (guint i = 0; picker->query_tokens[i]; i++) {
        const char *token = picker->query_tokens[i];
        if (*token && !strstr(record->search, token)) return FALSE;
    }
    return TRUE;
}

void anto_emoji_factory_bind(GtkSignalListItemFactory *factory, GtkListItem *item,
                         gpointer data) {
    (void)factory;
    (void)data;
    EmojiRecord *record = gtk_list_item_get_item(item);
    GtkWidget *card = gtk_list_item_get_child(item);
    GtkWidget *symbol = g_object_get_data(G_OBJECT(card), "emoji-symbol");
    gtk_label_set_text(GTK_LABEL(symbol), record->symbol);
    g_autofree char *tooltip = g_strdup_printf("%s  %s\n%s · %s\n%s",
                                               record->symbol, record->name,
                                               anto_emoji_localized_group(record->group),
                                               record->subgroup, record->keywords);
    gtk_widget_set_tooltip_text(card, tooltip);
}

void anto_emoji_copy_record(EmojiPicker *picker, guint position) {
    if (position == GTK_INVALID_LIST_POSITION) return;
    EmojiRecord *record =
        g_list_model_get_item(G_LIST_MODEL(picker->filtered), position);
    if (!record) return;
    const char *argv[] = {"wl-copy", NULL};
    if (menu_run_with_input(argv, record->symbol, NULL, NULL)) {
        menu_notify("Emoji copiata", record->symbol);
        menu_close(picker->app);
    } else {
        menu_notify("Emoji", "Impossibile copiare negli appunti");
    }
    g_object_unref(record);
}

void anto_emoji_update_footer(EmojiPicker *picker, const char *query) {
    guint visible = g_list_model_get_n_items(G_LIST_MODEL(picker->filtered));
    g_autofree char *count = g_strdup_printf(
        "%u %s su %u  ·  frecce navigano  ·  Invio copia  ·  Esc chiude",
        visible, visible == 1 ? "risultato" : "risultati", picker->total);
    if (query && *query && visible == 0)
        menu_set_footer(picker->app, "Nessuna emoji trovata · prova nome, categoria o parola chiave");
    else
        menu_set_footer(picker->app, count);
}

void anto_emoji_picker_free(gpointer data) {
    EmojiPicker *picker = data;
    if (!picker) return;
    if (picker->key_controller) {
        GtkWidget *widget = gtk_event_controller_get_widget(picker->key_controller);
        if (widget) gtk_widget_remove_controller(widget, picker->key_controller);
        g_clear_object(&picker->key_controller);
    }
    g_strfreev(picker->query_tokens);
    g_clear_object(&picker->selection);
    g_clear_object(&picker->filtered);
    g_clear_object(&picker->filter);
    g_free(picker);
}

void anto_emoji_open_characters(GtkButton *button, gpointer data) {
    (void)button;
    const char *argv[] = {"gnome-characters", NULL};
    menu_spawn(data, argv, TRUE);
}
