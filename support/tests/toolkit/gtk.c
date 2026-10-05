#include <gtk/gtk.h>
#if GTK_MAJOR_VERSION >= 4 && !defined(ANTO_PLAIN_GTK4)
#include <adwaita.h>
#endif
#include <stdio.h>

static unsigned errors = 0;
static GtkWidget *action;
static void css_error(GtkCssProvider *provider, GtkCssSection *section, GError *error, void *data) {
    (void)provider; (void)section; (void)data;
#if GTK_MAJOR_VERSION >= 4
    if (error->domain == GTK_CSS_PARSER_ERROR) errors++;
#else
    if (error->code != GTK_CSS_PROVIDER_ERROR_DEPRECATED) errors++;
#endif
    if (errors) fprintf(stderr, "CSS: %s\n", error->message);
}

static gboolean report(gpointer data) {
    GtkWidget *window = data;
    GdkRGBA accent, background, foreground, action_foreground, error;
    GtkStyleContext *context = gtk_widget_get_style_context(window);
    gtk_style_context_lookup_color(context, "accent_bg_color", &accent);
    gtk_style_context_lookup_color(context, "window_bg_color", &background);
    gtk_style_context_get_color(context,
#if GTK_MAJOR_VERSION < 4
        GTK_STATE_FLAG_NORMAL,
#endif
        &foreground);
    gtk_style_context_get_color(gtk_widget_get_style_context(action),
#if GTK_MAJOR_VERSION < 4
        GTK_STATE_FLAG_NORMAL,
#endif
        &action_foreground);
    gtk_style_context_lookup_color(context, "error_bg_color", &error);
    char *theme_name = NULL;
    g_object_get(gtk_settings_get_default(), "gtk-theme-name", &theme_name, NULL);
    FILE *file = fopen(g_getenv("ANTO_TOOLKIT_REPORT"), "w");
    fprintf(file, "{\"toolkit\":\"GTK%d\",\"themeName\":\"%s\",\"cssErrors\":%u,\"foregroundAlpha\":%.4f,\"windowAlpha\":%.4f,\"accent\":\"#%02x%02x%02x\",\"actionForeground\":\"#%02x%02x%02x\",\"error\":\"#%02x%02x%02x\"}", GTK_MAJOR_VERSION, theme_name ? theme_name : "", errors, foreground.alpha, background.alpha, (unsigned)(accent.red*255+.5), (unsigned)(accent.green*255+.5), (unsigned)(accent.blue*255+.5), (unsigned)(action_foreground.red*255+.5), (unsigned)(action_foreground.green*255+.5), (unsigned)(action_foreground.blue*255+.5), (unsigned)(error.red*255+.5), (unsigned)(error.green*255+.5), (unsigned)(error.blue*255+.5));
    g_free(theme_name);
    fclose(file);
    return G_SOURCE_REMOVE;
}

int main(int argc, char **argv) {
    g_set_prgname(
#ifdef ANTO_PLAIN_GTK4
        "anto-toolkit-gtk4plain"
#else
        GTK_MAJOR_VERSION >= 4 ? "anto-toolkit-gtk4" : "anto-toolkit-gtk3"
#endif
    );
#if GTK_MAJOR_VERSION >= 4
    gtk_init();
#ifndef ANTO_PLAIN_GTK4
    adw_init();
    adw_style_manager_set_color_scheme(adw_style_manager_get_default(), ADW_COLOR_SCHEME_FORCE_DARK);
#endif
#else
    gtk_init(&argc, &argv);
#endif
    GtkCssProvider *provider = gtk_css_provider_new();
    g_signal_connect(provider, "parsing-error", G_CALLBACK(css_error), NULL);
    gtk_css_provider_load_from_path(provider, argv[1]
#if GTK_MAJOR_VERSION < 4
        , NULL
#endif
    );
#if GTK_MAJOR_VERSION >= 4 && !defined(ANTO_PLAIN_GTK4)
    GtkWidget *window = adw_window_new();
    /* libadwaita supplies its maintained control theme; the installed user
     * palette overrides semantic roles without replacing that widget provider. */
#elif GTK_MAJOR_VERSION >= 4
    GtkWidget *window = gtk_window_new();
    /* Use the installed theme selected through GtkSettings, not a test override. */
#else
    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_wmclass(GTK_WINDOW(window), "anto-toolkit-gtk3", "anto-toolkit-gtk3");
    /* Parse the artifact above, but let GtkSettings load the full base normally. */
#endif
    gtk_window_set_title(GTK_WINDOW(window), "Anto426 toolkit theme test");
    gtk_window_set_default_size(GTK_WINDOW(window), 900, 580);
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 18);
    gtk_widget_set_margin_start(box, 32); gtk_widget_set_margin_end(box, 32);
    gtk_widget_set_margin_top(box, 32); gtk_widget_set_margin_bottom(box, 32);
    action = gtk_button_new_with_label("Shared accent and readable text");
    gtk_style_context_add_class(gtk_widget_get_style_context(action), "suggested-action");
    GtkWidget *scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0, 100, 1);
    gtk_range_set_value(GTK_RANGE(scale), 65);
    GtkWidget *check = gtk_check_button_new_with_label("Native checkbox and slider");
#if GTK_MAJOR_VERSION >= 4
    gtk_check_button_set_active(GTK_CHECK_BUTTON(check), TRUE);
#else
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check), TRUE);
#endif
#if GTK_MAJOR_VERSION >= 4 && !defined(ANTO_PLAIN_GTK4)
    GtkWidget *header = adw_header_bar_new();
    GtkWidget *toolbar = adw_toolbar_view_new();
    adw_toolbar_view_add_top_bar(ADW_TOOLBAR_VIEW(toolbar), header);
    adw_toolbar_view_set_content(ADW_TOOLBAR_VIEW(toolbar), box);
    adw_window_set_content(ADW_WINDOW(window), toolbar);
    GtkWidget *group = adw_preferences_group_new();
    adw_preferences_group_set_title(ADW_PREFERENCES_GROUP(group), "Current libadwaita controls");
    GtkWidget *toggle = adw_switch_row_new();
    adw_preferences_row_set_title(ADW_PREFERENCES_ROW(toggle), "Wallpaper palette");
    adw_switch_row_set_active(ADW_SWITCH_ROW(toggle), TRUE);
    adw_preferences_group_add(ADW_PREFERENCES_GROUP(group), toggle);
    GtkWidget *entry = adw_entry_row_new();
    adw_preferences_row_set_title(ADW_PREFERENCES_ROW(entry), "Readable input");
    adw_preferences_group_add(ADW_PREFERENCES_GROUP(group), entry);
    gtk_box_append(GTK_BOX(box), group);
    gtk_box_append(GTK_BOX(box), action);
    gtk_box_append(GTK_BOX(box), check);
    gtk_box_append(GTK_BOX(box), scale);
    gtk_window_present(GTK_WINDOW(window));
#elif GTK_MAJOR_VERSION >= 4
    gtk_window_set_child(GTK_WINDOW(window), box);
    gtk_box_append(GTK_BOX(box), gtk_label_new("Maintained full GTK4 widget theme"));
    GtkWidget *entry = gtk_entry_new(); gtk_entry_set_placeholder_text(GTK_ENTRY(entry), "A readable placeholder");
    gtk_box_append(GTK_BOX(box), entry);
    gtk_box_append(GTK_BOX(box), action);
    gtk_box_append(GTK_BOX(box), check);
    gtk_box_append(GTK_BOX(box), scale);
    gtk_window_present(GTK_WINDOW(window));
#else
    gtk_container_add(GTK_CONTAINER(window), box);
    GtkWidget *label = gtk_label_new("GTK3 palette, controls and transparent background");
    GtkWidget *entry = gtk_entry_new(); gtk_entry_set_placeholder_text(GTK_ENTRY(entry), "A readable placeholder");
    gtk_box_pack_start(GTK_BOX(box), label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), entry, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), action, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), check, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), scale, FALSE, FALSE, 0);
    gtk_widget_show_all(window);
#endif
    g_timeout_add(500, report, window);
#if GTK_MAJOR_VERSION >= 4
    GMainLoop *loop = g_main_loop_new(NULL, FALSE); g_main_loop_run(loop);
#else
    gtk_main();
#endif
    return errors ? 1 : 0;
}
