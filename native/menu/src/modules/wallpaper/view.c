#include "internal.h"
#include "gallery_page.h"
#include "design_tokens.h"

static void navigate(const char *page, gpointer data) {
    menu_open(data, page);
}

static void search(MenuApp *app, const char *query, gpointer page) {
    (void)app;
    anto_wallpaper_page_search(page, query);
}

static gboolean key(guint keyval, GdkModifierType modifiers, gpointer page) {
    return anto_wallpaper_page_key(page, keyval, modifiers);
}

void menu_show_wallpaper(MenuApp *app) {
    menu_page_begin(app, "preferences-desktop-wallpaper-symbolic", "Sfondi",
                     "Anteprima e raccolta · desktop, GRUB e login",
                     "Cerca nella raccolta…");
    int width = gtk_widget_get_width(app->custom_holder);
    if (width <= 0) width = ANTO_SIZE_MENU_WIDTH - ANTO_SIZE_SIDEBAR_WIDTH - 64;
    GtkWidget *page = anto_wallpaper_page_new(app->window, app->search, width,
                                             navigate, app);
    menu_set_custom_content(app, page);
    menu_set_search_action(app, search, page, NULL);
    app->key_action = key;
    app->key_data = page;
    menu_set_footer(app, "Frecce navigano · Invio applica · Ctrl+Invio GRUB e login · Esc chiude");
}
