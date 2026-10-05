#include <gtk/gtk.h>
#include <string.h>

static unsigned errors;
static void failed(GtkCssProvider *provider, GtkCssSection *section,
                   const GError *error, gpointer data) {
    (void)provider; (void)section; (void)data;
    g_printerr("CSS: %s\n", error->message); errors++;
}
static void append(GString *css, const char *path) {
    g_autofree char *text = NULL;
    if (!g_file_get_contents(path, &text, NULL, NULL)) { errors++; return; }
    g_auto(GStrv) lines = g_strsplit(text, "\n", -1);
    for (guint i = 0; lines[i]; i++)
        if (!g_str_has_prefix(lines[i], "@import")) g_string_append_printf(css, "%s\n", lines[i]);
}
int main(int argc, char **argv) {
    if (argc < 4) return 2;
    for (int i = 3; i < argc; i++) {
        GString *css = g_string_new(NULL);
        append(css, argv[1]); append(css, argv[2]); append(css, argv[i]);
        GtkCssProvider *provider = gtk_css_provider_new();
        g_signal_connect(provider, "parsing-error", G_CALLBACK(failed), NULL);
#if GTK_MAJOR_VERSION == 4
        gtk_css_provider_load_from_string(provider, css->str);
#else
        gtk_css_provider_load_from_data(provider, css->str, css->len, NULL);
#endif
        g_object_unref(provider); g_string_free(css, TRUE);
    }
    return errors ? 1 : 0;
}
