#include <gtk/gtk.h>

int main(int argc, char **argv) {
    GtkCssProvider *provider;

    if (argc != 2)
        return 2;

    provider = gtk_css_provider_new();
    gtk_css_provider_load_from_path(provider, argv[1]);
    g_object_unref(provider);
    return 0;
}
