#include "../src/modules/emoji/internal.h"
#include "primitives.h"

/* Production factory and keyboard behavior, with no access to the clipboard. */
gboolean menu_run_with_input(const char *const argv[], const char *input,
                             char **out, char **error) {
    (void)argv; (void)input; (void)out; (void)error;
    g_assert_not_reached();
}
void menu_notify(const char *title, const char *body) {
    (void)title; (void)body; g_assert_not_reached();
}
void menu_close(MenuApp *app) { (void)app; g_assert_not_reached(); }

static void settle(void) {
    gint64 until = g_get_monotonic_time() + 200000;
    while (g_get_monotonic_time() < until) {
        while (g_main_context_iteration(NULL, FALSE));
        g_usleep(1000);
    }
}

static GdkTexture *render(GtkWidget *window) {
    GdkPaintable *paintable = gtk_widget_paintable_new(window);
    GtkSnapshot *snapshot = gtk_snapshot_new();
    int width = gtk_widget_get_width(window), height = gtk_widget_get_height(window);
    gdk_paintable_snapshot(paintable, snapshot, width, height);
    GskRenderNode *node = gtk_snapshot_free_to_node(snapshot);
    graphene_rect_t viewport = GRAPHENE_RECT_INIT(0, 0, width, height);
    GdkTexture *texture = gsk_renderer_render_texture(
        gtk_native_get_renderer(GTK_NATIVE(window)), node, &viewport);
    gsk_render_node_unref(node);
    g_object_unref(paintable);
    return texture;
}

static void selection_pixels(GtkWidget *window, GtkWidget *grid) {
    GtkWidget *selected = NULL;
    for (GtkWidget *child = gtk_widget_get_first_child(grid); child;
         child = gtk_widget_get_next_sibling(child)) {
        if (gtk_widget_get_state_flags(child) & GTK_STATE_FLAG_SELECTED) {
            selected = child;
            break;
        }
    }
    g_assert_nonnull(selected);
    graphene_rect_t bounds;
    g_assert_true(gtk_widget_compute_bounds(selected, window, &bounds));
    g_autoptr(GdkTexture) texture = render(window);
    const char *image = g_getenv("ANTO426_EMOJI_FIXTURE_IMAGE");
    if (image) g_assert_true(gdk_texture_save_to_png(texture, image));
    g_autoptr(GdkTextureDownloader) downloader = gdk_texture_downloader_new(texture);
    gdk_texture_downloader_set_format(downloader, GDK_MEMORY_R8G8B8A8);
    gsize stride;
    g_autoptr(GBytes) bytes = gdk_texture_downloader_download_bytes(downloader, &stride);
    const guchar *pixels = g_bytes_get_data(bytes, NULL);
    int x = (int)bounds.origin.x + 4;
    int y = (int)(bounds.origin.y + bounds.size.height / 2);
    const guchar *background = pixels + y * stride + x * 4;
    /* A pink palette must replace the blue Adwaita selection rectangle. */
    g_assert_cmpuint(background[0], >, background[2] + 5);
    guint colour_pixels = 0;
    for (int row = bounds.origin.y; row < bounds.origin.y + bounds.size.height; row++)
        for (int column = bounds.origin.x; column < bounds.origin.x + bounds.size.width; column++) {
            const guchar *pixel = pixels + row * stride + column * 4;
            if (pixel[0] > 140 && pixel[1] > 90 && pixel[2] < 80) colour_pixels++;
        }
    /* U+263A must use the colour emoji font without changing its codepoints. */
    g_assert_cmpuint(colour_pixels, >, 25);
}

int main(int argc, char **argv) {
    if (!gtk_init_check()) return 77;
    for (int i = 1; i < argc; i++) {
        GtkCssProvider *style = gtk_css_provider_new();
        gtk_css_provider_load_from_path(style, argv[i]);
        gtk_style_context_add_provider_for_display(gdk_display_get_default(),
            GTK_STYLE_PROVIDER(style), GTK_STYLE_PROVIDER_PRIORITY_USER + i);
        g_object_unref(style);
    }
    GtkCssProvider *test = gtk_css_provider_new();
    gtk_css_provider_load_from_string(test,
        "@define-color accent #ed88b0; .emoji-test { background: #222222; }");
    gtk_style_context_add_provider_for_display(gdk_display_get_default(),
        GTK_STYLE_PROVIDER(test), GTK_STYLE_PROVIDER_PRIORITY_USER + 20);
    g_object_unref(test);
    g_autoptr(GListStore) catalog = g_list_store_new(emoji_record_get_type());
    const char *symbols[] = {"☺", "😀", "😘"};
    for (guint i = 0; i < 64; i++) {
        EmojiRecord *record = anto_emoji_record_new(symbols[i % 3], "Smileys & Emotion",
                                                   "faces", "smile", "happy");
        g_list_store_append(catalog, record);
        g_object_unref(record);
    }
    EmojiPicker picker = {0};
    picker.filtered = gtk_filter_list_model_new(G_LIST_MODEL(g_object_ref(catalog)), NULL);
    picker.selection = gtk_single_selection_new(G_LIST_MODEL(g_object_ref(picker.filtered)));
    picker.view = anto_emoji_create_grid(&picker);
    GtkWidget *window = gtk_window_new();
    gtk_widget_add_css_class(window, "emoji-test");
    gtk_window_set_default_size(GTK_WINDOW(window), 760, 300);
    gtk_window_set_child(GTK_WINDOW(window), anto_ui_scroller(picker.view));
    gtk_window_present(GTK_WINDOW(window));
    settle();
    g_assert_cmpuint(anto_emoji_columns(&picker), ==, 8);
    selection_pixels(window, picker.view);
    g_autoptr(GObject) record = g_list_model_get_item(G_LIST_MODEL(catalog), 0);
    g_assert_cmpstr(((EmojiRecord *)record)->symbol, ==, "☺");
    gtk_window_destroy(GTK_WINDOW(window));
    g_object_unref(picker.selection);
    picker.selection = gtk_single_selection_new(G_LIST_MODEL(g_object_ref(picker.filtered)));
    picker.view = anto_emoji_create_grid(&picker);
    window = gtk_window_new();
    gtk_widget_add_css_class(window, "emoji-test");
    gtk_window_set_default_size(GTK_WINDOW(window), 320, 300);
    gtk_window_set_child(GTK_WINDOW(window), anto_ui_scroller(picker.view));
    gtk_window_present(GTK_WINDOW(window));
    settle();
    guint columns = anto_emoji_columns(&picker);
    g_assert_cmpuint(columns, >=, 1);
    g_assert_cmpuint(columns, <, 8);
    g_assert_true(anto_emoji_key_pressed(NULL, GDK_KEY_Down, 0, 0, &picker));
    settle();
    g_assert_cmpuint(gtk_single_selection_get_selected(picker.selection), ==, columns);
    gtk_window_destroy(GTK_WINDOW(window));
    g_object_unref(picker.selection);
    g_object_unref(picker.filtered);
    g_print("emoji: palette selection, colour glyphs and responsive keyboard navigation passed\n");
    return 0;
}
