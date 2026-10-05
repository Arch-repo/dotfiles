#include "primitives.h"

typedef struct {
    int min_width, width, min_height, height;
    GdkRGBA colour;
} Appearance;

int main(int argc, char **argv) {
    if (!gtk_init_check()) return 77;
    for (int i = 1; i < argc; i++) {
        GtkCssProvider *provider = gtk_css_provider_new();
        gtk_css_provider_load_from_path(provider, argv[i]);
        gtk_style_context_add_provider_for_display(gdk_display_get_default(),
            GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_USER + i);
        g_object_unref(provider);
    }
    const char *surfaces[] = {"anto-menu-panel", "wallpaper-panel", "osd-panel"};
    const char *windows[] = {"anto-menu-window", "wallpaper-window", "anto-osd"};
    Appearance reference[10] = {0};
    for (guint surface = 0; surface < G_N_ELEMENTS(surfaces); surface++) {
        GtkWidget *window = gtk_window_new();
        gtk_widget_add_css_class(window, windows[surface]);
        GtkWidget *panel = anto_ui_stack(GTK_ORIENTATION_VERTICAL, 0, surfaces[surface]);
        gtk_window_set_child(GTK_WINDOW(window), panel);
        GtkWidget *controls[] = {
            anto_ui_action("Conferma", "object-select-symbolic", NULL),
            anto_ui_action("Applica", "object-select-symbolic", "primary"),
            anto_ui_icon_button("window-close-symbolic", "Chiudi"),
            anto_ui_switch(TRUE, "Attivo"),
            anto_ui_scale(30, 0, 100, 1, "Livello"),
            anto_ui_field("Titolo", "Scrivi qui", NULL),
            anto_ui_choice("Seleziona"),
            anto_ui_progress(),
            anto_ui_level(0, 100),
            anto_ui_row(anto_ui_icon("bluetooth-symbolic", ANTO_CONTROL_ROW_ICON, "item-icon"),
                        "Dispositivo", "Un dettaglio", NULL, NULL),
        };
        for (guint i = 0; i < G_N_ELEMENTS(controls); i++) {
            gtk_box_append(GTK_BOX(panel), controls[i]);
            Appearance value = {0};
            gtk_widget_measure(controls[i], GTK_ORIENTATION_HORIZONTAL, -1,
                               &value.min_width, &value.width, NULL, NULL);
            gtk_widget_measure(controls[i], GTK_ORIENTATION_VERTICAL, -1,
                               &value.min_height, &value.height, NULL, NULL);
            gtk_widget_get_color(controls[i], &value.colour);
            if (!surface) reference[i] = value;
            else {
                g_assert_cmpint(value.min_width, ==, reference[i].min_width);
                g_assert_cmpint(value.width, ==, reference[i].width);
                g_assert_cmpint(value.min_height, ==, reference[i].min_height);
                g_assert_cmpint(value.height, ==, reference[i].height);
                g_assert_true(gdk_rgba_equal(&value.colour, &reference[i].colour));
            }
        }
        gtk_window_destroy(GTK_WINDOW(window));
    }
    g_print("primitives: 10 controls have identical geometry and colours in all 3 native surfaces\n");
    return 0;
}
