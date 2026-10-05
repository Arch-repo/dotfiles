#include "internal.h"

gboolean anto_keyboard_view_is_current(KeyboardRuntime *runtime) {
    GtkWidget *root = g_weak_ref_get(&runtime->root);
    gboolean current =
        root && g_strcmp0(runtime->app->current_page, "keyboard") == 0 &&
        gtk_widget_get_parent(root) == runtime->app->list;
    g_clear_object(&root);
    return current;
}

gboolean anto_keyboard_row_search_set(GtkWidget *row, const char *title,
                                        const char *subtitle,
                                        const char *value,
                                        const char *keyboard) {
    if (!row) return FALSE;
    g_autofree char *combined = g_strdup_printf(
        "%s %s %s %s", title ? title : "", subtitle ? subtitle : "",
        value ? value : "", keyboard ? keyboard : "");
    g_autofree char *search = g_utf8_strdown(combined, -1);
    const char *current =
        g_object_get_data(G_OBJECT(row), "menu-search");
    if (g_strcmp0(current, search) == 0) return FALSE;
    g_object_set_data_full(G_OBJECT(row), "menu-search",
                           g_steal_pointer(&search), g_free);
    return TRUE;
}
