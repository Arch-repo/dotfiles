#include "glass.h"
#include <gdk/wayland/gdkwayland.h>
#include <wayland-client.h>
#include "ext-background-effect-v1-client.h"
#include "hyprglass-item-v1-client.h"
#include <math.h>
#include <string.h>

typedef struct {
    struct wl_compositor *compositor;
    struct ext_background_effect_manager_v1 *effects;
    struct hyprglass_item_manager_v1 *hints;
} GlassGlobals;
typedef struct {
    GtkWindow *window;
    GtkWidget *panel;
    float radius;
    GdkFrameClock *clock;
    gulong realize_handler;
    gulong unrealize_handler;
    gulong layout_handler;
    gulong paint_handler;
    graphene_rect_t bounds;
    gboolean valid_bounds;
    struct ext_background_effect_surface_v1 *effect;
    struct hyprglass_item_v1 *item;
} GlassSurface;
static GlassGlobals globals;

static void disconnect_handler(gpointer object, gulong *handler) {
    /* GTK may have disposed its signal closures before window qdata is freed. */
    if (object && *handler && g_signal_handler_is_connected(object, *handler))
        g_signal_handler_disconnect(object, *handler);
    *handler = 0;
}

static void release_surface(GlassSurface *glass) {
    disconnect_handler(glass->clock, &glass->layout_handler);
    disconnect_handler(glass->clock, &glass->paint_handler);
    g_clear_object(&glass->clock);
    if (glass->item) { hyprglass_item_v1_destroy(glass->item); glass->item = NULL; }
    if (glass->effect) { ext_background_effect_surface_v1_destroy(glass->effect); glass->effect = NULL; }
    glass->valid_bounds = FALSE;
}

static void unrealized(GtkWidget *widget, gpointer data) {
    (void)widget;
    release_surface(data);
}

static void capabilities(void *data, struct ext_background_effect_manager_v1 *manager, uint32_t flags) {
    (void)data; (void)manager; (void)flags;
}
static const struct ext_background_effect_manager_v1_listener effect_listener = {capabilities};
static void global(void *data, struct wl_registry *registry, uint32_t name, const char *interface, uint32_t version) {
    (void)version;
    GlassGlobals *state = data;
    if (!strcmp(interface, "wl_compositor"))
        state->compositor = wl_registry_bind(registry, name, &wl_compositor_interface, 1);
    else if (!strcmp(interface, "ext_background_effect_manager_v1")) {
        state->effects = wl_registry_bind(registry, name, &ext_background_effect_manager_v1_interface, 1);
        ext_background_effect_manager_v1_add_listener(state->effects, &effect_listener, NULL);
    } else if (!strcmp(interface, "hyprglass_item_manager_v1"))
        state->hints = wl_registry_bind(registry, name, &hyprglass_item_manager_v1_interface, 1);
}
static void removed(void *data, struct wl_registry *registry, uint32_t name) { (void)data; (void)registry; (void)name; }
static const struct wl_registry_listener listener = {global, removed};
static gboolean discover(GdkDisplay *display) {
    if (!GDK_IS_WAYLAND_DISPLAY(display)) return FALSE;
    if (globals.compositor) return globals.effects != NULL;
    struct wl_display *wayland = gdk_wayland_display_get_wl_display(display);
    struct wl_registry *registry = wl_display_get_registry(wayland);
    wl_registry_add_listener(registry, &listener, &globals);
    wl_display_roundtrip(wayland);
    wl_registry_destroy(registry);
    return globals.effects != NULL;
}
static void commit_effect(GdkFrameClock *clock, gpointer data) {
    GlassSurface *glass = data;
    GdkSurface *surface = gtk_native_get_surface(GTK_NATIVE(glass->window));
    /* Cached render nodes may produce no GTK buffer commit. Latch only the
     * protocol metadata after GTK's paint phase; buffers remain GTK-owned. */
    if (surface && GDK_IS_WAYLAND_SURFACE(surface) &&
        gtk_widget_get_mapped(GTK_WIDGET(glass->window))) {
        wl_surface_commit(gdk_wayland_surface_get_wl_surface(surface));
        wl_display_flush(gdk_wayland_display_get_wl_display(gdk_surface_get_display(surface)));
    }
    disconnect_handler(clock, &glass->paint_handler);
}
static void update(GdkFrameClock *clock, gpointer data) {
    GlassSurface *glass = data;
    GdkSurface *surface = gtk_native_get_surface(GTK_NATIVE(glass->window));
    graphene_rect_t bounds;
    if (!surface || !gtk_widget_get_mapped(GTK_WIDGET(glass->window)) ||
        !gtk_widget_compute_bounds(glass->panel, GTK_WIDGET(glass->window), &bounds) ||
        bounds.size.width < 100 || bounds.size.height < 40 ||
        gtk_widget_get_width(GTK_WIDGET(glass->window)) < 100) return;
    if (!GDK_IS_WAYLAND_SURFACE(surface) || !discover(gdk_surface_get_display(surface))) {
        return;
    }
    if (glass->valid_bounds && graphene_rect_equal(&glass->bounds, &bounds)) return;
    glass->bounds = bounds;
    glass->valid_bounds = TRUE;
    struct wl_surface *wayland_surface = gdk_wayland_surface_get_wl_surface(surface);
    if (!glass->effect) glass->effect = ext_background_effect_manager_v1_get_background_effect(globals.effects, wayland_surface);
    struct wl_region *region = wl_compositor_create_region(globals.compositor);
    wl_region_add(region, (int)floorf(bounds.origin.x), (int)floorf(bounds.origin.y),
        (int)ceilf(bounds.size.width), (int)ceilf(bounds.size.height));
    ext_background_effect_surface_v1_set_blur_region(glass->effect, region);
    wl_region_destroy(region);
    if (globals.hints) {
        if (!glass->item) glass->item = hyprglass_item_manager_v1_get_item(globals.hints, wayland_surface);
        wl_fixed_t radius = wl_fixed_from_double(glass->radius);
        hyprglass_item_v1_set_shape(glass->item, wl_fixed_from_double(bounds.origin.x), wl_fixed_from_double(bounds.origin.y),
            wl_fixed_from_double(bounds.size.width), wl_fixed_from_double(bounds.size.height), radius, radius, radius, radius);
    }
    if (!glass->paint_handler) {
        glass->paint_handler = g_signal_connect_after(clock, "after-paint", G_CALLBACK(commit_effect), glass);
    }
    gtk_widget_queue_draw(glass->panel);
    gdk_surface_queue_render(surface);
    if (g_getenv("ANTO426_GLASS_TRACE"))
        g_message("Glass region %.1f,%.1f %.1fx%.1f radius=%.1f", bounds.origin.x, bounds.origin.y,
            bounds.size.width, bounds.size.height, glass->radius);
}
static void realized(GtkWidget *widget, gpointer data) {
    GlassSurface *glass = data;
    GdkSurface *surface = gtk_native_get_surface(GTK_NATIVE(widget));
    if (!surface || !GDK_IS_WAYLAND_SURFACE(surface) ||
        !discover(gdk_surface_get_display(surface))) return;
    struct wl_surface *wayland = gdk_wayland_surface_get_wl_surface(surface);
    glass->effect = ext_background_effect_manager_v1_get_background_effect(globals.effects, wayland);
    /* Establish an empty mask before the first buffer. The actual bounds are
     * sent after GTK layout, before paint commits that buffer. */
    struct wl_region *region = wl_compositor_create_region(globals.compositor);
    ext_background_effect_surface_v1_set_blur_region(glass->effect, region);
    wl_region_destroy(region);
    glass->clock = g_object_ref(gtk_widget_get_frame_clock(widget));
    glass->layout_handler = g_signal_connect_after(glass->clock, "layout", G_CALLBACK(update), glass);
    gdk_frame_clock_request_phase(glass->clock, GDK_FRAME_CLOCK_PHASE_LAYOUT);
}
static void destroy(gpointer data) {
    GlassSurface *glass = data;
    disconnect_handler(glass->window, &glass->realize_handler);
    disconnect_handler(glass->window, &glass->unrealize_handler);
    release_surface(glass);
    g_free(glass);
}
void anto_glass_bind(GtkWindow *window, GtkWidget *panel, float radius) {
    GlassSurface *glass = g_object_get_data(G_OBJECT(window), "anto-glass");
    if (!glass) {
        glass = g_new0(GlassSurface, 1); glass->window = window;
        g_object_set_data_full(G_OBJECT(window), "anto-glass", glass, destroy);
        glass->realize_handler = g_signal_connect(window, "realize", G_CALLBACK(realized), glass);
        glass->unrealize_handler = g_signal_connect(window, "unrealize", G_CALLBACK(unrealized), glass);
        if (gtk_widget_get_realized(GTK_WIDGET(window))) realized(GTK_WIDGET(window), glass);
    }
    glass->panel = panel; glass->radius = radius;
    glass->valid_bounds = FALSE;
    if (glass->clock) gdk_frame_clock_request_phase(glass->clock, GDK_FRAME_CLOCK_PHASE_LAYOUT);
}
void anto_load_glass_style(GdkDisplay *display) {
    g_autofree char *path = g_build_filename(g_get_home_dir(), ".local/share/anto-desktop/glass.css", NULL);
    if (!g_file_test(path, G_FILE_TEST_IS_REGULAR)) return;
    GtkCssProvider *style = gtk_css_provider_new();
    gtk_css_provider_load_from_path(style, path);
    gtk_style_context_add_provider_for_display(display, GTK_STYLE_PROVIDER(style), GTK_STYLE_PROVIDER_PRIORITY_USER + 5);
    g_object_unref(style);
}
