#include "internal.h"

void anto_brightness_scale_pressed(GtkGestureClick *gesture, int n_press,
                                     double x, double y, gpointer data) {
    (void)gesture;
    (void)n_press;
    (void)x;
    (void)y;
    BrightnessLive *live = data;
    live->pointer_active = TRUE;
    live->scale_active = TRUE;
    if (live->interaction_source) {
        g_source_remove(live->interaction_source);
        live->interaction_source = 0;
    }
}

void anto_brightness_scale_released(GtkGestureClick *gesture, int n_press,
                                      double x, double y, gpointer data) {
    (void)gesture;
    (void)n_press;
    (void)x;
    (void)y;
    BrightnessLive *live = data;
    live->pointer_active = FALSE;
    if (live->interaction_source) {
        g_source_remove(live->interaction_source);
        live->interaction_source = 0;
    }
    live->scale_active = FALSE;
    menu_brightness_live_event(live->app);
}

gboolean anto_brightness_interaction_fire(gpointer data) {
    BrightnessLive *live = data;
    live->interaction_source = 0;
    if (live->pointer_active)
        return G_SOURCE_REMOVE;
    live->scale_active = FALSE;
    if (g_strcmp0(live->app->current_page, "brightness") == 0)
        anto_brightness_refresh_start(live);
    return G_SOURCE_REMOVE;
}
