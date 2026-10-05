#include "size_bin.h"
#include <stdio.h>
int main(void) {
    if (!gtk_init_check()) return 77;
    GBytes *pixels = g_bytes_new_take(g_malloc0(3000 * 1800 * 3), 3000 * 1800 * 3);
    GdkTexture *texture = gdk_memory_texture_new(3000, 1800, GDK_MEMORY_R8G8B8, pixels, 3000 * 3);
    g_bytes_unref(pixels);
    GtkWidget *picture = gtk_picture_new_for_paintable(GDK_PAINTABLE(texture));
    gtk_picture_set_can_shrink(GTK_PICTURE(picture), TRUE);
    gtk_widget_set_halign(picture, GTK_ALIGN_FILL);
    gtk_widget_set_valign(picture, GTK_ALIGN_FILL);
    GtkWidget *bin = anto_size_bin_new(picture, 980, 610);
    g_object_ref_sink(bin);
    int minimum, natural;
    gtk_widget_measure(bin, GTK_ORIENTATION_HORIZONTAL, -1, &minimum, &natural, NULL, NULL);
    gboolean valid = minimum == 980 && natural == 980;
    gtk_widget_measure(bin, GTK_ORIENTATION_VERTICAL, 980, &minimum, &natural, NULL, NULL);
    valid &= minimum == 610 && natural == 610;
    g_object_unref(bin);g_object_unref(texture);
    if (!valid) fprintf(stderr, "Large preview expanded the fixed gallery bounds\n");
    return valid ? 0 : 1;
}
