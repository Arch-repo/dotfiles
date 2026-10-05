#include "internal.h"
#include "primitives.h"

const char *anto_emoji_localized_search_terms(const char *group) {
    if (g_strcmp0(group, "Smileys & Emotion") == 0)
        return "faccine emozioni";
    if (g_strcmp0(group, "People & Body") == 0)
        return "persone corpo";
    if (g_strcmp0(group, "Animals & Nature") == 0)
        return "animali natura piante fiori";
    if (g_strcmp0(group, "Food & Drink") == 0)
        return "cibo bevande mangiare bere";
    if (g_strcmp0(group, "Travel & Places") == 0)
        return "viaggi luoghi trasporti edifici";
    if (g_strcmp0(group, "Activities") == 0)
        return "attività sport gioco festa";
    if (g_strcmp0(group, "Objects") == 0)
        return "oggetti strumenti tecnologia";
    if (g_strcmp0(group, "Symbols") == 0)
        return "simboli segni frecce";
    if (g_strcmp0(group, "Flags") == 0)
        return "bandiere paesi nazioni";
    if (g_strcmp0(group, "Component") == 0)
        return "varianti tonalità pelle capelli";
    return "emoji";
}

void anto_emoji_factory_setup(GtkSignalListItemFactory *factory, GtkListItem *item,
                          gpointer data) {
    (void)factory;
    (void)data;
    GtkWidget *symbol;
    GtkWidget *card = anto_ui_glyph_tile("", &symbol);

    g_object_set_data(G_OBJECT(card), "emoji-symbol", symbol);
    gtk_list_item_set_child(item, card);
}

GtkWidget *anto_emoji_create_grid(EmojiPicker *picker) {
    GtkListItemFactory *factory = gtk_signal_list_item_factory_new();
    g_signal_connect(factory, "setup", G_CALLBACK(anto_emoji_factory_setup), NULL);
    g_signal_connect(factory, "bind", G_CALLBACK(anto_emoji_factory_bind), NULL);
    GtkWidget *view = gtk_grid_view_new(
        GTK_SELECTION_MODEL(g_object_ref(picker->selection)), factory);
    gtk_widget_add_css_class(view, "ui-collection");
    gtk_widget_set_hexpand(view, TRUE);
    gtk_widget_set_vexpand(view, TRUE);
    gtk_grid_view_set_min_columns(GTK_GRID_VIEW(view), 1);
    gtk_grid_view_set_max_columns(GTK_GRID_VIEW(view), EMOJI_GRID_COLUMNS);
    gtk_grid_view_set_single_click_activate(GTK_GRID_VIEW(view), TRUE);
    return view;
}

guint anto_emoji_columns(EmojiPicker *picker) {
    /* Read the allocated row, so keyboard navigation follows responsive GTK
     * layout rather than assuming the wide-screen column count. */
    guint columns = 0;
    float first_y = 0;
    for (GtkWidget *child = gtk_widget_get_first_child(picker->view); child;
         child = gtk_widget_get_next_sibling(child)) {
        graphene_rect_t bounds;
        if (!gtk_widget_compute_bounds(child, picker->view, &bounds) ||
            bounds.size.width <= 0 || bounds.size.height <= 0) continue;
        if (!columns) first_y = bounds.origin.y;
        if (bounds.origin.y > first_y + 1) break;
        columns++;
    }
    return columns ? columns : EMOJI_GRID_COLUMNS;
}

void menu_show_emoji(MenuApp *app) {
    GListStore *source = anto_emoji_get_catalog();
    guint total = g_list_model_get_n_items(G_LIST_MODEL(source));
    g_autofree char *subtitle = anto_emoji_catalog_is_fallback
        ? g_strdup("Archivio di sistema assente: set essenziale pronto all’uso")
        : g_strdup_printf("%u emoji, simboli e varianti · ricerca nell’archivio completo",
                          total);
    menu_page_begin(app, "face-smile-symbolic", "Emoji e simboli", subtitle,
                    "Cerca emoji, nome, categoria o parola chiave…");

    EmojiPicker *picker = g_new0(EmojiPicker, 1);
    picker->app = app;
    picker->total = total;
    picker->filter = gtk_custom_filter_new(anto_emoji_matches, picker, NULL);
    picker->filtered = gtk_filter_list_model_new(
        G_LIST_MODEL(g_object_ref(source)), GTK_FILTER(g_object_ref(picker->filter)));
    picker->selection = gtk_single_selection_new(
        G_LIST_MODEL(g_object_ref(picker->filtered)));
    gtk_single_selection_set_autoselect(picker->selection, TRUE);
    gtk_single_selection_set_can_unselect(picker->selection, FALSE);

    picker->view = anto_emoji_create_grid(picker);
    g_signal_connect(picker->view, "activate", G_CALLBACK(anto_emoji_activated), picker);

    GtkWidget *scroll = anto_ui_scroller(picker->view);
    gtk_widget_set_hexpand(scroll, TRUE);
    GtkWidget *content = anto_ui_stack(GTK_ORIENTATION_VERTICAL, ANTO_SPACING_SM, NULL);
    gtk_widget_set_hexpand(content, TRUE);
    gtk_widget_set_vexpand(content, TRUE);
    g_autofree char *characters = g_find_program_in_path("gnome-characters");
    if (anto_emoji_catalog_is_fallback && characters) {
        GtkWidget *button = anto_ui_action("Apri l’archivio Unicode completo in Caratteri", "accessories-character-map-symbolic", NULL);
        gtk_widget_add_css_class(button, "emoji-fallback-button");
        g_signal_connect(button, "clicked", G_CALLBACK(anto_emoji_open_characters), app);
        gtk_box_append(GTK_BOX(content), button);
    }
    gtk_box_append(GTK_BOX(content), scroll);
    menu_set_custom_content(app, content);
    menu_set_search_action(app, anto_emoji_search_changed, picker, anto_emoji_picker_free);

    GtkEventController *key_controller = gtk_event_controller_key_new();
    /* gtk_widget_add_controller() takes ownership.  Keep a separate strong
     * reference so picker teardown is safe even when the search widget has
     * already disposed its controllers while the window is closing. */
    picker->key_controller = g_object_ref(key_controller);
    gtk_event_controller_set_propagation_phase(picker->key_controller, GTK_PHASE_CAPTURE);
    g_signal_connect(picker->key_controller, "key-pressed",
                     G_CALLBACK(anto_emoji_key_pressed), picker);
    gtk_widget_add_controller(app->search, key_controller);
    anto_emoji_update_footer(picker, "");
}
