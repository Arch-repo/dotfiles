#include "primitives.h"
#include "glass.h"
#include "monitor.h"
#include <gtk4-layer-shell.h>

static void settle(void) {
    gint64 until = g_get_monotonic_time() + 120000;
    while (g_get_monotonic_time() < until) {
        while (g_main_context_iteration(NULL, FALSE));
        g_usleep(1000);
    }
}

static void transparent_host(GtkWidget *window) {
    GdkPaintable *paintable = gtk_widget_paintable_new(window);
    GtkSnapshot *snapshot = gtk_snapshot_new();
    int width = gtk_widget_get_width(window), height = gtk_widget_get_height(window);
    gdk_paintable_snapshot(paintable, snapshot, width, height);
    GskRenderNode *node = gtk_snapshot_free_to_node(snapshot);
    g_assert_nonnull(node);
    graphene_rect_t viewport = GRAPHENE_RECT_INIT(0, 0, width, height);
    GdkTexture *texture = gsk_renderer_render_texture(gtk_native_get_renderer(GTK_NATIVE(window)), node, &viewport);
    g_assert_nonnull(texture);
    g_autofree guchar *pixels = g_malloc0(width * height * 4);
    gdk_texture_download(texture, pixels, width * 4);
    g_assert_cmpuint(pixels[(height / 2 * width) * 4 + 3], ==, 0);
    /* Ensure this is a rendered panel, rather than an entirely empty image. */
    g_assert_cmpuint(pixels[(height / 2 * width + width / 2) * 4 + 3], >, 50);
    g_object_unref(texture);
    gsk_render_node_unref(node);
    g_object_unref(paintable);
}

int main(void) {
    if (!gtk_init_check()) return 77;
    g_log_set_always_fatal(G_LOG_FATAL_MASK | G_LOG_LEVEL_CRITICAL);
    anto_ui_init(gdk_display_get_default());
    anto_load_glass_style(gdk_display_get_default());
    for (int iteration = 0; iteration < 3; iteration++) {
        GtkWidget *window = gtk_window_new();
        g_object_ref(window);
        gtk_window_set_decorated(GTK_WINDOW(window), FALSE);
        gtk_window_set_default_size(GTK_WINDOW(window), 600, 400);
        gtk_widget_add_css_class(window, "anto-osd");
        gtk_widget_set_state_flags(window, GTK_STATE_FLAG_FOCUS_VISIBLE, FALSE);
        if (anto_layer_shell_supported(gdk_display_get_default())) {
            gtk_layer_init_for_window(GTK_WINDOW(window));
            gtk_layer_set_namespace(GTK_WINDOW(window), "anto426-osd");
            gtk_layer_set_layer(GTK_WINDOW(window), GTK_LAYER_SHELL_LAYER_OVERLAY);
        }
        GtkWidget *panel = anto_ui_card(GTK_ORIENTATION_VERTICAL);
        gtk_widget_add_css_class(panel, "osd-panel");
        gtk_widget_set_size_request(panel, 400, 220);
        gtk_widget_set_halign(panel, GTK_ALIGN_CENTER);
        gtk_widget_set_valign(panel, GTK_ALIGN_CENTER);
        gtk_box_append(GTK_BOX(panel), anto_ui_text("Materiale della shell", "item-title", 1));
        GtkWidget *root = gtk_overlay_new();
        gtk_overlay_set_child(GTK_OVERLAY(root), anto_ui_stack(GTK_ORIENTATION_VERTICAL, 0, NULL));
        gtk_overlay_add_overlay(GTK_OVERLAY(root), panel);
        gtk_window_set_child(GTK_WINDOW(window), root);
        anto_glass_bind(GTK_WINDOW(window), panel, ANTO_RADIUS_PANEL);
        gtk_window_present(GTK_WINDOW(window));
        settle();
        g_assert_true(gtk_widget_get_state_flags(window) & GTK_STATE_FLAG_FOCUS_VISIBLE);
        transparent_host(window);
        gtk_widget_set_visible(window, FALSE);
        gtk_widget_unrealize(window);
        settle();
        gtk_window_present(GTK_WINDOW(window));
        settle();
        gtk_window_destroy(GTK_WINDOW(window));
        g_object_unref(window);
        settle();
    }
    g_print("glass lifecycle: transparent focused host, realize, unrealize, remap and destroy passed\n");
    return 0;
}
