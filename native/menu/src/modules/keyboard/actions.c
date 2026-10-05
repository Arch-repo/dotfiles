#include "internal.h"

void menu_keyboard_live_event(MenuApp *app) {
    if (!app || !app->window ||
        g_strcmp0(app->current_page, "keyboard") != 0)
        return;
    anto_keyboard_refresh_start(anto_keyboard_runtime_get(app));
}

void menu_show_keyboard(MenuApp *app) {
    KeyboardRuntime *runtime = anto_keyboard_runtime_get(app);
    for (guint index = 0;
         index < G_N_ELEMENTS(runtime->layout_badges); index++)
        runtime->layout_badges[index] = NULL;
    for (guint index = 0;
         index < G_N_ELEMENTS(runtime->layout_rows); index++)
        runtime->layout_rows[index] = NULL;
    runtime->fcitx_badge = NULL;
    runtime->fcitx_row = NULL;
    g_weak_ref_set(&runtime->root, NULL);

    g_autofree char *subtitle = anto_keyboard_subtitle(runtime->snapshot);
    menu_page_begin(app, "input-keyboard-symbolic", "Tastiera",
                    subtitle, "Cerca layout o impostazione…");
    menu_add_shell_item(app, "media-skip-forward-symbolic",
                        "Layout successivo",
                        "Cambia layout su tutte le tastiere", "N/D",
                        "hyprctl switchxkblayout all next", TRUE);
    GtkListBoxRow *root =
        gtk_list_box_get_row_at_index(GTK_LIST_BOX(app->list), 0);
    if (root) {
        g_weak_ref_set(&runtime->root, G_OBJECT(root));
        runtime->layout_rows[0] = GTK_WIDGET(root);
    }
    menu_add_shell_item(app, "media-skip-backward-symbolic",
                        "Layout precedente",
                        "Cambia layout su tutte le tastiere", "N/D",
                        "hyprctl switchxkblayout all prev", TRUE);
    menu_add_shell_item(
        app, "preferences-desktop-keyboard-symbolic",
        "Configura input method",
        "Lingue, metodi di input e scorciatoie fcitx5", "N/D",
        "fcitx5-configtool", TRUE);
    runtime->layout_rows[1] = GTK_WIDGET(
        gtk_list_box_get_row_at_index(GTK_LIST_BOX(app->list), 1));
    runtime->fcitx_row = GTK_WIDGET(
        gtk_list_box_get_row_at_index(GTK_LIST_BOX(app->list), 2));
    menu_add_shell_item(app, "view-refresh-symbolic",
                        "Riavvia fcitx5",
                        "Ricarica il demone di input della sessione", NULL,
                        "pkill -x fcitx5 || true; fcitx5 -d", TRUE);
    menu_add_nav(
        app, "preferences-desktop-keyboard-shortcuts-symbolic",
        "Scorciatoie Hyprland", "Consulta la mappa completa", NULL,
        "shortcuts");
    menu_set_footer(
        app, "Layout e stato input method cambiano in-place");

    runtime->layout_badges[0] = anto_keyboard_row_badge_at(app, 0);
    runtime->layout_badges[1] = anto_keyboard_row_badge_at(app, 1);
    runtime->fcitx_badge = anto_keyboard_row_badge_at(app, 2);
    anto_keyboard_apply_snapshot(runtime, runtime->snapshot);
    anto_keyboard_refresh_start(runtime);
}
