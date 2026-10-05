#include "internal.h"

gboolean anto_floating_view_is_current(FloatingRuntime *runtime) {
    GtkWidget *root = g_weak_ref_get(&runtime->root);
    gboolean current =
        root && g_strcmp0(runtime->app->current_page, "floating") == 0 &&
        gtk_widget_get_parent(root) == runtime->app->grid;
    g_clear_object(&root);
    return current;
}

void menu_show_floating(MenuApp *app) {
    FloatingRuntime *runtime = anto_floating_runtime_get(app);
    runtime->floating_badge = NULL;
    runtime->pin_badge = NULL;
    g_weak_ref_set(&runtime->root, NULL);

    g_autofree char *subtitle = anto_floating_subtitle(runtime->snapshot);
    menu_page_begin(app, "focus-windows-symbolic", "Finestre", subtitle,
                    "Cerca un’azione finestra…");
    menu_set_layout(app, MENU_LAYOUT_GRID);
    menu_set_grid_columns(app, 3);
    menu_add_shell_tile(app, "focus-windows-symbolic", "Floating on / off",
                        "Alterna tra tiled e fluttuante", "N/D",
                        "hyprctl dispatch togglefloating", TRUE);
    menu_add_shell_tile(app, "view-restore-symbolic", "Centra finestra",
                        "Sposta la finestra al centro del monitor", NULL,
                        "hyprctl dispatch centerwindow", TRUE);
    menu_add_shell_tile(app, "view-pin-symbolic", "Pin / unpin",
                        "Mantiene la finestra su tutti i workspace", "N/D",
                        "hyprctl dispatch pin", TRUE);
    menu_add_shell_tile(app, "go-top-symbolic", "Porta in primo piano",
                        "Alza la finestra sopra le altre", NULL,
                        "hyprctl dispatch alterzorder top", TRUE);
    menu_add_shell_tile(app, "zoom-out-symbolic", "Compatta · 640 × 420",
                        "Ideale per terminali e utility", NULL,
                        "hyprctl dispatch resizeactive exact 640 420; hyprctl dispatch centerwindow", TRUE);
    menu_add_shell_tile(app, "zoom-original-symbolic", "Comoda · 920 × 640",
                        "Dimensione quotidiana bilanciata", "DEFAULT",
                        "hyprctl dispatch resizeactive exact 920 640; hyprctl dispatch centerwindow", TRUE);
    menu_add_shell_tile(app, "zoom-in-symbolic", "Grande · 1280 × 820",
                        "Spazio ampio senza fullscreen", NULL,
                        "hyprctl dispatch resizeactive exact 1280 820; hyprctl dispatch centerwindow", TRUE);
    menu_add_shell_tile(app, "edit-undo-symbolic", "Ripristina tiled",
                        "Rimuove pin e torna al layout", NULL,
                        "hyprctl dispatch pin off; hyprctl dispatch settiled", TRUE);
    menu_add_shell_tile(app, "window-close-symbolic", "Chiudi finestra",
                        "Invia killactive a Hyprland", NULL,
                        "hyprctl dispatch killactive", TRUE);
    menu_add_nav_tile(app, "utilities-system-monitor-symbolic",
                      "Processi in background",
                      "Ispeziona il resto della sessione", NULL,
                      "background");
    menu_set_footer(
        app, "Stato finestra aggiornato in-place · nessun salto della vista");

    GtkFlowBoxChild *root =
        gtk_flow_box_get_child_at_index(GTK_FLOW_BOX(app->grid), 0);
    if (root)
        g_weak_ref_set(&runtime->root, G_OBJECT(root));
    runtime->floating_badge = anto_floating_tile_badge_at(app, 0);
    runtime->pin_badge = anto_floating_tile_badge_at(app, 2);
    anto_floating_apply_snapshot(runtime, runtime->snapshot);
    anto_floating_refresh_start(runtime);
}
