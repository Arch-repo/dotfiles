#include "components.h"
#include <string.h>
GtkWidget *menu_ui_static_row(GtkWidget *child) {
    GtkWidget *row = gtk_list_box_row_new();
    gtk_list_box_row_set_selectable(GTK_LIST_BOX_ROW(row), FALSE);
    gtk_list_box_row_set_activatable(GTK_LIST_BOX_ROW(row), FALSE);
    gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), child);
    return row;
}
static void release(gpointer data) {
    RowAction *action = data;
    if (action->destroy) action->destroy(action->data);
    g_free(action);
}
void menu_ui_bind(GtkWidget *widget, MenuApp *app, MenuAction callback, gpointer data, GDestroyNotify destroy) {
    if (!callback) { if (destroy) destroy(data); return; }
    RowAction *binding = g_new0(RowAction, 1);
    *binding = (RowAction){callback, app, data, destroy};
    g_object_set_data_full(G_OBJECT(widget), "menu-action", binding, release);
    gtk_widget_add_css_class(widget, "activatable");
}
void menu_ui_search(GtkWidget *widget, const char *title, const char *subtitle, const char *badge) {
    g_autofree char *text = g_strdup_printf("%s %s %s", title ? title : "", subtitle ? subtitle : "", badge ? badge : "");
    g_object_set_data_full(G_OBJECT(widget), "menu-search", g_utf8_strdown(text, -1), g_free);
}
GtkWidget *menu_ui_item_icon(const char *name, int size) {
    if (name && g_str_has_prefix(name, "text:")) return anto_ui_text(name + 5, "emoji-icon", 1);
    if (name && g_path_is_absolute(name) && g_file_test(name, G_FILE_TEST_IS_REGULAR)) {
        GtkWidget *picture = anto_ui_picture(name);
        gtk_picture_set_content_fit(GTK_PICTURE(picture), GTK_CONTENT_FIT_COVER);
        gtk_widget_set_size_request(picture, size + 30, size);
        gtk_widget_add_css_class(picture, "wallpaper-thumb");
        return picture;
    }
    return anto_ui_icon(name && *name ? name : "application-x-executable-symbolic", size, "item-icon");
}
