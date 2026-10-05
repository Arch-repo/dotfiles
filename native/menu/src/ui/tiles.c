#include "components.h"
static void navigate(MenuApp *app, gpointer page) { menu_open(app, page); }
void menu_add_tile(MenuApp *app, const char *icon, const char *title, const char *subtitle,
                   const char *badge, MenuAction callback, gpointer data, GDestroyNotify destroy) {
    GtkWidget *child = gtk_flow_box_child_new();
    gtk_widget_add_css_class(child, "menu-tile");
    gtk_widget_set_valign(child, GTK_ALIGN_START);
    GtkWidget *card = anto_ui_tile(menu_ui_item_icon(icon, ANTO_SIZE_ICON_LARGE), title, subtitle, badge);
    gtk_widget_add_css_class(card, "tile-card");
    gtk_flow_box_child_set_child(GTK_FLOW_BOX_CHILD(child), card);
    menu_ui_bind(child, app, callback, data, destroy);
    menu_ui_search(child, title, subtitle, badge);
    gtk_accessible_update_property(GTK_ACCESSIBLE(child), GTK_ACCESSIBLE_PROPERTY_LABEL, title ? title : "",
                                   GTK_ACCESSIBLE_PROPERTY_DESCRIPTION, subtitle ? subtitle : "", -1);
    gtk_flow_box_insert(GTK_FLOW_BOX(app->grid), child, -1);
}
void menu_add_nav_tile(MenuApp *app, const char *icon, const char *title, const char *subtitle,
                       const char *badge, const char *page) {
    menu_add_tile(app, icon, title, subtitle, badge, navigate, g_strdup(page), g_free);
}
