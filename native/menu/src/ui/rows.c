#include "components.h"
static void navigate(MenuApp *app, gpointer page) { menu_open(app, page); }
static void row(MenuApp *app, const char *icon, const char *title, const char *subtitle,
                const char *badge, MenuAction callback, gpointer data, GDestroyNotify destroy, gboolean nav) {
    GtkWidget *body = anto_ui_row(menu_ui_item_icon(icon, ANTO_CONTROL_ROW_ICON), title, subtitle, NULL, NULL);
    if (badge && *badge) {
        GtkWidget *status = anto_ui_badge(badge);
        gtk_widget_add_css_class(status, "item-badge");
        gtk_box_append(GTK_BOX(body), status);
    }
    if (nav) gtk_box_append(GTK_BOX(body), anto_ui_icon("go-next-symbolic", 12, "chevron"));
    GtkWidget *widget = menu_ui_static_row(body);
    gtk_widget_add_css_class(widget, "setting-item");
    gtk_list_box_row_set_activatable(GTK_LIST_BOX_ROW(widget), callback != NULL);
    gtk_list_box_row_set_selectable(GTK_LIST_BOX_ROW(widget), callback != NULL);
    menu_ui_bind(widget, app, callback, data, destroy);
    menu_ui_search(widget, title, subtitle, badge);
    gtk_list_box_append(GTK_LIST_BOX(app->list), widget);
}
void menu_add_item(MenuApp *app, const char *icon, const char *title, const char *subtitle,
                   const char *badge, MenuAction callback, gpointer data, GDestroyNotify destroy) {
    row(app, icon, title, subtitle, badge, callback, data, destroy, FALSE);
}
void menu_add_nav(MenuApp *app, const char *icon, const char *title, const char *subtitle,
                  const char *badge, const char *page) {
    row(app, icon, title, subtitle, badge, navigate, g_strdup(page), g_free, TRUE);
}
