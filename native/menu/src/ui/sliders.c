#include "components.h"
static void release(gpointer data, GClosure *closure) {
    (void)closure;
    ScaleAction *binding = data;
    if (binding->destroy) binding->destroy(binding->data);
    g_free(binding);
}
static void changed(GtkRange *range, gpointer data) {
    ScaleAction *binding = data;
    if (binding->callback) binding->callback(binding->app, gtk_range_get_value(range), binding->data);
}
void menu_add_scale(MenuApp *app, const char *icon, const char *title, const char *subtitle,
                    double value, double min, double max, double step, MenuScaleAction callback,
                    gpointer data, GDestroyNotify destroy) {
    GtkWidget *scale = NULL;
    GtkWidget *body = anto_ui_slider_card(icon, title, subtitle, value, min, max, step, NULL, NULL, &scale);
    ScaleAction *binding = g_new0(ScaleAction, 1);
    *binding = (ScaleAction){callback, app, data, destroy};
    g_signal_connect_data(scale, "value-changed", G_CALLBACK(changed), binding, release, 0);
    menu_append_widget(app, body);
}
