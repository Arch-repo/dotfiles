#include "shell.h"
static void back(GtkButton *button, gpointer data) { (void)button; menu_back(data); }
static void close_menu(GtkButton *button, gpointer data) { (void)button; menu_close(data); }
GtkWidget *menu_ui_header(MenuApp *app) {
    GtkWidget *header = anto_ui_stack(GTK_ORIENTATION_VERTICAL, ANTO_SPACING_LG, "menu-header");
    GtkWidget *bar = anto_ui_stack(GTK_ORIENTATION_HORIZONTAL, ANTO_SPACING_MD, NULL);
    app->back_button = anto_ui_icon_button("go-previous-symbolic", "Indietro");
    g_signal_connect(app->back_button, "clicked", G_CALLBACK(back), app);
    gtk_box_append(GTK_BOX(bar), app->back_button);
    GtkWidget *titles = anto_ui_heading("preferences-system-symbolic", "Desktop", "", &app->page_icon, &app->page_title, &app->page_subtitle);
    gtk_box_append(GTK_BOX(bar), titles);
    GtkWidget *close = anto_ui_icon_button("window-close-symbolic", "Chiudi · Esc");
    g_signal_connect(close, "clicked", G_CALLBACK(close_menu), app);
    gtk_box_append(GTK_BOX(bar), close);
    gtk_box_append(GTK_BOX(header), bar);
    app->search = anto_ui_search("Cerca…");
    gtk_search_entry_set_key_capture_widget(GTK_SEARCH_ENTRY(app->search), GTK_WIDGET(app->window));
    g_signal_connect(app->search, "search-changed", G_CALLBACK(search_changed), app);
    gtk_box_append(GTK_BOX(header), app->search);
    return header;
}
