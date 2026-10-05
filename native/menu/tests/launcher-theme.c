/* Exercise the real launcher with a desktop entry and a child environment probe. */
#include "../src/modules/launcher/internal.h"
#include <glib/gstdio.h>

static gboolean closed;
void menu_close(MenuApp *app) { (void)app; closed = TRUE; }
void menu_notify(const char *title, const char *body) { g_error("%s: %s", title, body); }

int main(int argc, char **argv) {
    if (argc == 3 && g_str_equal(argv[1], "--probe")) {
        g_assert_null(g_getenv("GTK_THEME"));
        g_assert_cmpstr(g_getenv("ANTO_LAUNCH_TEST"), ==, "preserved");
        g_assert_true(g_file_set_contents(argv[2], "desktop theme inherited", -1, NULL));
        return 0;
    }
    g_autofree char *directory = g_dir_make_tmp("anto-launch-theme-XXXXXX", NULL);
    g_assert_nonnull(directory);
    g_autofree char *report = g_build_filename(directory, "child.txt", NULL);
    g_autofree char *executable = g_file_read_link("/proc/self/exe", NULL);
    g_autofree char *command = g_strdup_printf("\"%s\" --probe \"%s\"", executable, report);
    g_autoptr(GKeyFile) desktop = g_key_file_new();
    g_key_file_set_string(desktop, "Desktop Entry", "Type", "Application");
    g_key_file_set_string(desktop, "Desktop Entry", "Name", "Theme environment probe");
    g_key_file_set_string(desktop, "Desktop Entry", "Exec", command);
    g_key_file_set_boolean(desktop, "Desktop Entry", "DBusActivatable", FALSE);
    g_autoptr(GDesktopAppInfo) info = g_desktop_app_info_new_from_keyfile(desktop);
    g_assert_nonnull(info);
    g_setenv("GTK_THEME", "Adwaita:dark", TRUE);
    g_setenv("ANTO_LAUNCH_TEST", "preserved", TRUE);
    MenuApp app = {0};
    AppEntry entry = {.info = G_APP_INFO(info)};
    anto_launcher_launch_app(&app, &entry);
    g_assert_true(closed);
    const gint64 deadline = g_get_monotonic_time() + 3 * G_TIME_SPAN_SECOND;
    while (!g_file_test(report, G_FILE_TEST_IS_REGULAR) && g_get_monotonic_time() < deadline)
        g_usleep(10000);
    g_assert_true(g_file_test(report, G_FILE_TEST_IS_REGULAR));
    /* The shell still owns its private base, without exporting it to children. */
    g_assert_cmpstr(g_getenv("GTK_THEME"), ==, "Adwaita:dark");
    g_remove(report);
    g_rmdir(directory);
    return 0;
}
