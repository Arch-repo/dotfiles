#include "menu.h"
#include "design_tokens.h"
#include "../src/core/ui_internal.h"
#include <glib/gstdio.h>

static void settle(int milliseconds) {
    gint64 until = g_get_monotonic_time() + milliseconds * 1000;
    while (g_get_monotonic_time() < until) {
        while (g_main_context_iteration(NULL, FALSE));
        g_usleep(1000);
    }
}
static GtkWidget *find(GtkWidget *root, const char *role) {
    if (gtk_widget_has_css_class(root, role)) return root;
    for (GtkWidget *child = gtk_widget_get_first_child(root); child; child = gtk_widget_get_next_sibling(child)) {
        GtkWidget *match = find(child, role);
        if (match) return match;
    }
    return NULL;
}
static void write_file(const char *path, const char *text, int mode) {
    g_autofree char *directory = g_path_get_dirname(path);
    g_assert_cmpint(g_mkdir_with_parents(directory, 0700), ==, 0);
    g_assert_true(g_file_set_contents(path, text, -1, NULL));
    g_assert_cmpint(g_chmod(path, mode), ==, 0);
}
static void same_bounds(GtkWidget *widget, GtkWidget *root, graphene_rect_t expected) {
    graphene_rect_t actual;
    g_assert_true(gtk_widget_compute_bounds(widget, root, &actual));
    g_assert_cmpfloat_with_epsilon(actual.origin.x, expected.origin.x, .1);
    g_assert_cmpfloat_with_epsilon(actual.origin.y, expected.origin.y, .1);
    g_assert_cmpfloat_with_epsilon(actual.size.width, expected.size.width, .1);
    g_assert_cmpfloat_with_epsilon(actual.size.height, expected.size.height, .1);
}
static void allocate(MenuApp *app) {
    // Broadway has no browser acknowledging frames; run the real bounded
    // allocation after each stack change instead of inspecting a stale frame.
    GtkWidget *bounds = g_object_get_data(G_OBJECT(app->deck), "panel-bounds");
    gtk_widget_queue_allocate(bounds);
    gtk_widget_allocate(bounds, ANTO_SIZE_MENU_WIDTH, ANTO_SIZE_MENU_HEIGHT, -1, NULL);
}
static void capture(const char *name) {
    const char *directory = g_getenv("ANTO426_WALLPAPER_FIXTURE_CAPTURE");
    if (!directory) return;
    g_autofree char *image = g_build_filename(directory, name, NULL);
    const char *argv[] = {"grim", "-o", g_getenv("ANTO426_TARGET_MONITOR"), image, NULL};
    GSubprocess *process = g_subprocess_newv(argv, G_SUBPROCESS_FLAGS_NONE, NULL);
    g_assert_nonnull(process);
    g_assert_true(g_subprocess_wait_check(process, NULL, NULL));
    g_object_unref(process);
}
static gboolean press(MenuApp *app, guint key, GdkModifierType modifiers) {
    g_autoptr(GListModel) controllers = gtk_widget_observe_controllers(GTK_WIDGET(app->window));
    for (guint i = 0; i < g_list_model_get_n_items(controllers); i++) {
        g_autoptr(GtkEventController) controller = g_list_model_get_item(controllers, i);
        if (!GTK_IS_EVENT_CONTROLLER_KEY(controller) ||
            gtk_event_controller_get_propagation_phase(controller) != GTK_PHASE_CAPTURE ||
            !g_signal_handler_find(controller, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, app))
            continue;
        gboolean handled = FALSE;
        g_signal_emit_by_name(controller, "key-pressed", key, 0, modifiers, &handled);
        return handled;
    }
    g_assert_not_reached();
}
static gboolean requested_close(GtkWindow *window, gpointer data) {
    (void)window;
    (*(guint *)data)++;
    return TRUE; // Inspect the close request without tearing down the shared fixture.
}

int main(int argc, char **argv) {
    g_assert_cmpint(argc, ==, 2);
    if (!gtk_init_check()) return 77;
    g_setenv("ANTO426_WALLPAPER_WORKER", argv[1], TRUE);
    g_autofree char *collection = g_build_filename(g_get_user_cache_dir(), "collection", NULL);
    g_assert_cmpint(g_mkdir_with_parents(collection, 0700), ==, 0);
    g_setenv("ANTO426_WALLPAPERS_DIR", collection, TRUE);
    for (int i = 0; i < 3; i++) {
        g_autofree char *image = g_strdup_printf("%s/%c.png", collection, 'A' + i);
        GdkPixbuf *pixels = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, 480, 320);
        const guint32 colors[] = {0xff4466ff, 0x3399eeff, 0x44bb66ff};
        gdk_pixbuf_fill(pixels, colors[i]);
        g_assert_true(gdk_pixbuf_save(pixels, image, "png", NULL, NULL));
        g_object_unref(pixels);
    }
    g_autofree char *backend = g_build_filename(g_get_home_dir(), ".local/libexec/anto-menu/anto-menu-backend", NULL);
    write_file(backend, "#!/bin/sh\nprintf 'volume\\t25%%\\nnetwork\\tFixture\\nbluetooth\\tSpento\\nbrightness\\t50%%\\ndisplays\\t1 attivo\\nbattery\\t80%%\\n'\n", 0700);
    MenuApp app = {0};
    app.history = g_ptr_array_new_with_free_func(g_free);
    app.application = gtk_application_new("com.anto426.WallpaperMenuFixture", G_APPLICATION_NON_UNIQUE);
    g_assert_true(g_application_register(G_APPLICATION(app.application), NULL, NULL));
    menu_build_window(&app);
    gtk_window_present(app.window);
    menu_open(&app, "system");
    settle(250);
    graphene_rect_t deck, sidebar;
    g_assert_true(gtk_widget_compute_bounds(app.deck, GTK_WIDGET(app.window), &deck));
    g_assert_true(gtk_widget_compute_bounds(app.nav_rail, app.deck, &sidebar));

    for (int cycle = 0; cycle < 3; cycle++) {
        menu_open(&app, "system");
        menu_open(&app, "wallpaper");
        GtkWidget *page = gtk_widget_get_first_child(app.custom_holder);
        GtkWidget *grid = find(page, "wallpaper-grid");
        GtkSingleSelection *selection = GTK_SINGLE_SELECTION(gtk_grid_view_get_model(GTK_GRID_VIEW(grid)));
        for (int i = 0; i < 40 && g_list_model_get_n_items(G_LIST_MODEL(selection)) != 3; i++) settle(25);
        g_assert_cmpuint(g_list_model_get_n_items(G_LIST_MODEL(selection)), ==, 3);
        settle(150);
        allocate(&app);
        g_assert_cmpuint(g_list_length(gtk_application_get_windows(app.application)), ==, 1);
        same_bounds(app.deck, GTK_WIDGET(app.window), deck);
        same_bounds(app.nav_rail, app.deck, sidebar);
        g_assert_null(find(page, "wallpaper-preview"));
        graphene_rect_t grid_bounds;
        g_assert_true(gtk_widget_compute_bounds(grid, page, &grid_bounds));
        g_assert_cmpfloat(grid_bounds.size.width, >, gtk_widget_get_width(page) * .9);
        if (cycle == 0) { settle(250); capture("legacy-preview-a.png"); }
        gtk_editable_set_text(GTK_EDITABLE(app.search), "impossibile");
        settle(200);
        g_assert_cmpuint(g_list_model_get_n_items(G_LIST_MODEL(selection)), ==, 0);
        g_assert_null(find(page, "wallpaper-boot-action"));
        g_assert_null(find(page, "wallpaper-footer"));
        gtk_editable_set_text(GTK_EDITABLE(app.search), "");
        settle(200);
        const guint arrows[] = {GDK_KEY_Right, GDK_KEY_Left, GDK_KEY_Down, GDK_KEY_Up};
        int columns = gtk_grid_view_get_max_columns(GTK_GRID_VIEW(grid));
        const guint expected[] = {1, 2, (guint)(columns % 3), (guint)((3 - columns % 3) % 3)};
        for (guint i = 0; i < G_N_ELEMENTS(arrows); i++) {
            gtk_single_selection_set_selected(selection, 0);
            gtk_widget_grab_focus(app.search);
            g_assert_true(press(&app, arrows[i], 0));
            g_assert_cmpuint(gtk_single_selection_get_selected(selection), ==, expected[i]);
            settle(30);
            GtkWidget *focus = gtk_window_get_focus(app.window);
            g_assert_true(focus && (focus == grid || gtk_widget_is_ancestor(focus, grid)));
        }
        gtk_single_selection_set_selected(selection, 0);
        gtk_widget_grab_focus(grid);
        g_assert_true(press(&app, GDK_KEY_Down, 0));
        g_assert_cmpuint(gtk_single_selection_get_selected(selection), ==, 2);
        if (cycle == 0) { settle(250); capture("legacy-preview-b.png"); }
        gtk_widget_grab_focus(app.search);
        g_assert_true(press(&app, GDK_KEY_Return, 0));
        menu_back(&app);
        g_assert_cmpstr(app.current_page, ==, "system");
        g_assert_null(app.key_action);
        g_assert_null(app.key_data);
        settle(100);
        // Leaving while catalogue/preview work is still pending is safe too.
        menu_open(&app, "wallpaper");
        menu_open(&app, "settings");
        settle(100);
        g_assert_cmpuint(g_list_length(gtk_application_get_windows(app.application)), ==, 1);
        same_bounds(app.deck, GTK_WIDGET(app.window), deck);
    }

    guint close_requests = 0;
    gulong close_handler = g_signal_connect(app.window, "close-request", G_CALLBACK(requested_close), &close_requests);
    const char *pages[] = {"system", "settings", "wallpaper"};
    for (guint i = 0; i < G_N_ELEMENTS(pages); i++) {
        menu_open(&app, pages[i]);
        gtk_editable_set_text(GTK_EDITABLE(app.search), "ricerca attiva");
        gtk_widget_grab_focus(app.search);
        guint history_size = app.history->len;
        g_assert_cmpuint(history_size, >, 0);
        g_assert_true(press(&app, GDK_KEY_Escape, 0));
        g_assert_cmpuint(close_requests, ==, i * 2 + 1);
        g_assert_cmpstr(app.current_page, ==, pages[i]);
        g_assert_cmpuint(app.history->len, ==, history_size);
        g_assert_cmpstr(gtk_editable_get_text(GTK_EDITABLE(app.search)), ==, "ricerca attiva");
        gtk_editable_set_text(GTK_EDITABLE(app.search), "");
        gtk_widget_grab_focus(app.back_button);
        g_assert_true(press(&app, GDK_KEY_Escape, 0));
        g_assert_cmpuint(close_requests, ==, i * 2 + 2);
        g_assert_cmpstr(app.current_page, ==, pages[i]);
        g_assert_cmpuint(app.history->len, ==, history_size);
    }
    g_signal_handler_disconnect(app.window, close_handler);
    menu_open(&app, "wallpaper");
    settle(300);
    g_autofree char *boot_script = g_build_filename(g_get_user_cache_dir(), "boot-worker", NULL);
    g_autofree char *result = g_build_filename(g_get_user_cache_dir(), "boot-result", NULL);
    g_autofree char *release = g_build_filename(g_get_user_cache_dir(), "release", NULL);
    write_file(boot_script, "#!/bin/sh\nwhile [ ! -e \"$FIXTURE_RELEASE\" ]; do sleep .02; done\nprintf '%s\\n' \"$ANTO426_WALLPAPER_OUTPUT\" \"$1\" > \"$FIXTURE_RESULT\"\n", 0700);
    g_setenv("ANTO426_WALLPAPER_BOOT_APPLY_SCRIPT", boot_script, TRUE);
    g_setenv("FIXTURE_RESULT", result, TRUE);
    g_setenv("FIXTURE_RELEASE", release, TRUE);
    g_object_ref(app.window);
    g_assert_true(app.key_action(GDK_KEY_Return, GDK_CONTROL_MASK, app.key_data));
    g_assert_false(gtk_widget_get_mapped(GTK_WIDGET(app.window)));
    g_assert_false(g_file_test(result, G_FILE_TEST_EXISTS));
    write_file(release, "\n", 0600);
    for (int i = 0; i < 100 && !g_file_test(result, G_FILE_TEST_EXISTS); i++) settle(20);
    g_autofree char *applied = NULL;
    g_assert_true(g_file_get_contents(result, &applied, NULL, NULL));
    g_assert_true(g_str_has_prefix(applied, "__boot_login__\n"));
    app.closing = TRUE;
    g_source_remove(app.clock_source);
    g_source_remove(app.status_source);
    anto_query_close(app.summary);
    g_clear_object(&app.summary);
    gtk_window_destroy(app.window);
    g_object_unref(app.window);
    g_ptr_array_free(app.rail_buttons, TRUE);
    g_ptr_array_free(app.history, TRUE);
    g_free(app.current_page);
    g_free(app.desktop_snapshot);
    g_object_unref(app.application);
    settle(200);
    g_print("wallpaper menu: one production window, fixed geometry, arrows from search, immediate preview, global Escape close, history, pending-task disposal and background boot-only action verified\n");
    return 0;
}
