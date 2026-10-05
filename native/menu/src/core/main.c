#include "menu.h"
#include "frontend.h"

#include <string.h>

static void stop_runtime(MenuApp *app) {
    if (!app) return;
    app->closing = TRUE;
    menu_live_stop(app);
    if (app->clock_source) {
        g_source_remove(app->clock_source);
        app->clock_source = 0;
    }
    if (app->status_source) {
        g_source_remove(app->status_source);
        app->status_source = 0;
    }
    anto_query_close(app->summary);
}

static gboolean close_requested(GtkWindow *window, gpointer data) {
    (void)window;
    stop_runtime(data);
    return FALSE;
}

static void activate(GtkApplication *application, gpointer data) {
    MenuApp *app = data;
    app->application = application;
    const char *page = app->requested_page && *app->requested_page ? app->requested_page : "apps";
    if (!app->window) {
        menu_build_window(app);
        g_signal_connect(app->window, "close-request",
                         G_CALLBACK(close_requested), app);
        menu_live_start(app);
    }
    menu_target_active_monitor(app);
    if (!app->current_page) menu_open(app, page);
    else if (g_strcmp0(app->current_page, page) != 0) menu_open(app, page);
    anto_frontend_present(app->window);
}

static int command_line(GApplication *application, GApplicationCommandLine *command, gpointer data) {
    MenuApp *app = data;
    int argc = 0;
    g_auto(GStrv) argv = g_application_command_line_get_arguments(command, &argc);
    g_free(app->requested_page);
    app->requested_page = g_strdup(argc > 1 ? argv[1] : "apps");
    g_application_activate(application);
    return 0;
}

int main(int argc, char **argv) {
    if (g_getenv("WAYLAND_DISPLAY")) g_setenv("GDK_BACKEND", "wayland", TRUE);
    if (argc>1 && g_strcmp0(argv[1],"--list-pages")==0) { menu_print_pages(); return 0; }
    if (argc>1 && g_strcmp0(argv[1],"--version")==0) { g_print("Anto Desktop 0.1.0 · GTK %u.%u.%u\n",gtk_get_major_version(),gtk_get_minor_version(),gtk_get_micro_version()); return 0; }
    int direct = menu_run_direct_action(argc, argv);
    if (direct >= 0) return direct;
    if(argc>1 && !menu_known_page(argv[1])) { g_printerr("Sezione non disponibile: %s\n",argv[1]);return 2; }

    /* The deck owns its complete visual language.  A stable GTK base avoids
     * inheriting unsupported rules from third-party desktop themes; the
     * wallpaper palette is still layered on top by ui.c. */
    if (!g_getenv("GTK_THEME")) g_setenv("GTK_THEME", "Adwaita:dark", FALSE);

    MenuApp app = {0};
    app.history = g_ptr_array_new_with_free_func(g_free);
    GtkApplication *application = gtk_application_new(
        "com.anto426.NativeMenu", G_APPLICATION_HANDLES_COMMAND_LINE);
    g_signal_connect(application, "activate", G_CALLBACK(activate), &app);
    g_signal_connect(application, "command-line", G_CALLBACK(command_line), &app);
    int status = g_application_run(G_APPLICATION(application), argc, argv);
    stop_runtime(&app);
    g_clear_object(&app.summary);
    g_free(app.desktop_snapshot);
    if (app.rail_buttons) g_ptr_array_free(app.rail_buttons, TRUE);
    g_clear_object(&application);
    g_ptr_array_free(app.history, TRUE);
    g_free(app.current_page);
    g_free(app.requested_page);
    return status;
}
