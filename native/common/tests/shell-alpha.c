#include "primitives.h"

static void settle(void) {
    gint64 until = g_get_monotonic_time() + 180000;
    while (g_get_monotonic_time() < until) {
        while (g_main_context_iteration(NULL, FALSE));
        g_usleep(1000);
    }
}

static void silhouette(GtkWidget *window, GtkWidget *pane) {
    graphene_rect_t bounds;
    g_assert_true(gtk_widget_compute_bounds(pane, window, &bounds));
    int width = gtk_widget_get_width(window), height = gtk_widget_get_height(window);
    GdkPaintable *paintable = gtk_widget_paintable_new(window);
    GtkSnapshot *snapshot = gtk_snapshot_new();
    gdk_paintable_snapshot(paintable, snapshot, width, height);
    GskRenderNode *node = gtk_snapshot_free_to_node(snapshot);
    g_assert_nonnull(node);
    graphene_rect_t viewport = GRAPHENE_RECT_INIT(0, 0, width, height);
    GdkTexture *texture = gsk_renderer_render_texture(gtk_native_get_renderer(GTK_NATIVE(window)), node, &viewport);
    g_assert_nonnull(texture);
    g_autofree guchar *pixels = g_malloc0(width * height * 4);
    gdk_texture_download(texture, pixels, width * 4);
    int left = bounds.origin.x, top = bounds.origin.y;
    int right = left + bounds.size.width, bottom = top + bounds.size.height;
    int cx = (left + right) / 2, cy = (top + bottom) / 2;
    int clear[][2] = {{left - 3, cy}, {right + 2, cy}, {cx, top - 3}, {cx, bottom + 2},
                     {left + 2, top + 2}, {right - 3, top + 2},
                     {left + 2, bottom - 3}, {right - 3, bottom - 3}};
    for (guint i = 0; i < G_N_ELEMENTS(clear); i++)
        g_assert_cmpuint(pixels[(clear[i][1] * width + clear[i][0]) * 4 + 3], ==, 0);
    /* The actual material must be present in the middle of the pane. */
    g_assert_cmpuint(pixels[(cy * width + cx) * 4 + 3], >, 50);
    g_object_unref(texture);
    gsk_render_node_unref(node);
    g_object_unref(paintable);
}

int main(int argc, char **argv) {
    if (!gtk_init_check()) return 77;
    g_log_set_always_fatal(G_LOG_FATAL_MASK | G_LOG_LEVEL_CRITICAL);
    for (int i = 1; i < argc; i++) {
        GtkCssProvider *provider = gtk_css_provider_new();
        gtk_css_provider_load_from_path(provider, argv[i]);
        gtk_style_context_add_provider_for_display(gdk_display_get_default(),
            GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_USER + i);
        g_object_unref(provider);
    }
    GtkCssProvider *host = gtk_css_provider_new();
    gtk_css_provider_load_from_string(host, "window.shell-alpha-test { background: transparent; }");
    gtk_style_context_add_provider_for_display(gdk_display_get_default(),
        GTK_STYLE_PROVIDER(host), GTK_STYLE_PROVIDER_PRIORITY_USER + 20);
    g_object_unref(host);
    for (int surface = 0; surface < 3; surface++) {
        GtkWidget *window = gtk_window_new();
        gtk_window_set_decorated(GTK_WINDOW(window), FALSE);
        gtk_widget_add_css_class(window, "shell-alpha-test");
        gtk_window_set_default_size(GTK_WINDOW(window), 600, 260);
        GtkWidget *root = gtk_overlay_new();
        gtk_overlay_set_child(GTK_OVERLAY(root), anto_ui_stack(GTK_ORIENTATION_VERTICAL, 0, NULL));
        GtkWidget *pane = anto_ui_stack(GTK_ORIENTATION_HORIZONTAL, 0, NULL);
        GtkWidget *holder = pane;
        gtk_widget_set_size_request(pane, 240, surface ? 100 : 66);
        if (surface == 0) {
            gtk_widget_set_name(pane, "workspaces");
            GtkWidget *first = anto_ui_text("eDP·1", NULL, 1);
            GtkWidget *current = anto_ui_text("eDP·4", NULL, 1);
            gtk_widget_set_name(first, "custom-workspace-first");
            gtk_widget_set_name(current, "custom-workspace-current");
            gtk_widget_add_css_class(current, "active");
            gtk_box_append(GTK_BOX(pane), first);
            gtk_box_append(GTK_BOX(pane), current);
        } else {
            gtk_box_append(GTK_BOX(pane), anto_ui_text("Materiale della shell", NULL, 1));
            if (surface == 1) {
                gtk_widget_add_css_class(pane, "notification");
                holder = anto_ui_stack(GTK_ORIENTATION_VERTICAL, 0, "notification-row");
                GtkWidget *background = anto_ui_stack(GTK_ORIENTATION_VERTICAL, 0, "notification-background");
                gtk_box_append(GTK_BOX(background), pane);
                gtk_box_append(GTK_BOX(holder), background);
            } else gtk_widget_add_css_class(pane, "control-center");
        }
        gtk_widget_set_halign(holder, GTK_ALIGN_CENTER);
        gtk_widget_set_valign(holder, GTK_ALIGN_CENTER);
        gtk_overlay_add_overlay(GTK_OVERLAY(root), holder);
        gtk_window_set_child(GTK_WINDOW(window), root);
        gtk_window_present(GTK_WINDOW(window));
        settle();
        silhouette(window, pane);
        gtk_window_destroy(GTK_WINDOW(window));
    }
    g_print("shell alpha: active workspace, toast and notification center have no outer halo\n");
    return 0;
}
