#include "size_bin.h"

typedef struct { GtkWidget parent; GtkWidget *child; int width, height; } AntoSizeBin;
typedef GtkWidgetClass AntoSizeBinClass;
G_DEFINE_TYPE(AntoSizeBin, anto_size_bin, GTK_TYPE_WIDGET)

static void measure(GtkWidget *widget, GtkOrientation orientation, int for_size,
                    int *minimum, int *natural, int *minimum_baseline, int *natural_baseline) {
    (void)for_size;
    AntoSizeBin *bin = (AntoSizeBin *)widget;
    int desired = orientation == GTK_ORIENTATION_HORIZONTAL ? bin->width : bin->height;
    *minimum = *natural = MAX(0, desired);
    *minimum_baseline = *natural_baseline = -1;
}
static void allocate(GtkWidget *widget, int width, int height, int baseline) {
    AntoSizeBin *bin = (AntoSizeBin *)widget;
    if (bin->child) gtk_widget_allocate(bin->child, width, height, baseline, NULL);
}
static void snapshot(GtkWidget *widget, GtkSnapshot *image) {
    AntoSizeBin *bin = (AntoSizeBin *)widget;
    if (!bin->child) return;
    int w = gtk_widget_get_width(widget);
    int h = gtk_widget_get_height(widget);
    if (gtk_widget_get_overflow(widget) == GTK_OVERFLOW_HIDDEN && w > 0 && h > 0) {
        graphene_rect_t rect = GRAPHENE_RECT_INIT(0, 0, (float)w, (float)h);
        gtk_snapshot_push_clip(image, &rect);
        gtk_widget_snapshot_child(widget, bin->child, image);
        gtk_snapshot_pop(image);
    } else {
        gtk_widget_snapshot_child(widget, bin->child, image);
    }
}
static void dispose(GObject *object) {
    AntoSizeBin *bin = (AntoSizeBin *)object;
    if (bin->child) {gtk_widget_unparent(bin->child);bin->child = NULL;}
    G_OBJECT_CLASS(anto_size_bin_parent_class)->dispose(object);
}
static void anto_size_bin_class_init(AntoSizeBinClass *class) {
    class->measure = measure;class->size_allocate = allocate;class->snapshot = snapshot;
    G_OBJECT_CLASS(class)->dispose = dispose;
}
static void anto_size_bin_init(AntoSizeBin *bin) {
    gtk_widget_set_overflow(GTK_WIDGET(bin), GTK_OVERFLOW_HIDDEN);
}
GtkWidget *anto_size_bin_new(GtkWidget *child, int width, int height) {
    AntoSizeBin *bin = g_object_new(anto_size_bin_get_type(), NULL);
    bin->width = width;bin->height = height;bin->child = child;
    gtk_widget_set_parent(child, GTK_WIDGET(bin));
    return GTK_WIDGET(bin);
}
void anto_size_bin_set_size(GtkWidget *widget, int width, int height) {
    g_return_if_fail(G_TYPE_CHECK_INSTANCE_TYPE(widget, anto_size_bin_get_type()));
    AntoSizeBin *bin = (AntoSizeBin *)widget;
    if (bin->width == width && bin->height == height) return;
    bin->width = width; bin->height = height;
    gtk_widget_queue_resize(widget);
}
