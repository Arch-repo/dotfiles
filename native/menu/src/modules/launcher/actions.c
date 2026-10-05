#include "internal.h"

void anto_launcher_launch_app(MenuApp *app, gpointer data) {
    AppEntry *entry = data;
    g_autoptr(GError) error = NULL;
    g_autoptr(GDesktopAppInfo) terminal_info = NULL;
    g_autoptr(GAppLaunchContext) context = g_app_launch_context_new();
    /* The shell's private GTK base must not override an application's theme. */
    g_app_launch_context_unsetenv(context, "GTK_THEME");
    GAppInfo *info = entry ? entry->info : NULL;
    if (info && G_IS_DESKTOP_APP_INFO(info) && g_desktop_app_info_get_boolean(G_DESKTOP_APP_INFO(info), "Terminal")) {
        g_autoptr(GKeyFile) file = g_key_file_new();
        const char *filename = g_desktop_app_info_get_filename(G_DESKTOP_APP_INFO(info));
        if (filename && g_key_file_load_from_file(file, filename, G_KEY_FILE_NONE, &error)) {
            g_autofree char *original = g_key_file_get_string(file, "Desktop Entry", "Exec", NULL);
            g_autofree char *command = g_strdup_printf("ghostty -e %s", original ? original : "");
            g_key_file_set_string(file, "Desktop Entry", "Exec", command);
            g_key_file_set_boolean(file, "Desktop Entry", "Terminal", FALSE);
            g_key_file_set_boolean(file, "Desktop Entry", "DBusActivatable", FALSE);
            terminal_info = g_desktop_app_info_new_from_keyfile(file);
            info = terminal_info ? G_APP_INFO(terminal_info) : info;
        }
    }
    if (!info || !g_app_info_launch(info, NULL, context, &error)) {
        menu_notify("Applicazioni", error ? error->message : "Avvio non riuscito");
        return;
    }
    menu_close(app);
}

char *anto_launcher_app_icon_key(GAppInfo *info) {
    GIcon *icon = g_app_info_get_icon(info);
    if (!icon) return g_strdup("themed:application-x-executable-symbolic");

    char *serialized = g_icon_to_string(icon);
    if (serialized && *serialized) return serialized;
    g_free(serialized);
    return g_strdup("themed:application-x-executable-symbolic");
}
