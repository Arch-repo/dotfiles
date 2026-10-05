#include "shell.h"
#include "size_bin.h"

/* Exercise the production shell with synthetic content and no system services. */
void load_css(void) {
    anto_ui_init(gdk_display_get_default());
    g_autofree char *file = g_build_filename(g_get_home_dir(), ".local/share/anto-desktop/menu.css", NULL);
    GtkCssProvider *css = gtk_css_provider_new();
    gtk_css_provider_load_from_path(css, file);
    gtk_style_context_add_provider_for_display(gdk_display_get_default(), GTK_STYLE_PROVIDER(css),
                                             GTK_STYLE_PROVIDER_PRIORITY_USER + 4);
    g_object_unref(css);
}
void menu_context_start(MenuApp *app) { (void)app; }
void menu_open(MenuApp *app, const char *page) { (void)app; (void)page; }
void menu_back(MenuApp *app) { (void)app; }
void menu_close(MenuApp *app) { gtk_window_close(app->window); }
void search_changed(GtkSearchEntry *entry, gpointer app) { (void)entry; (void)app; }
gboolean row_filter(GtkListBoxRow *row, gpointer app) { (void)row; (void)app; return TRUE; }
gboolean tile_filter(GtkFlowBoxChild *child, gpointer app) { (void)child; (void)app; return TRUE; }
void row_activated(GtkListBox *box, GtkListBoxRow *row, gpointer app) { (void)box; (void)row; (void)app; }
void tile_activated(GtkFlowBox *box, GtkFlowBoxChild *child, gpointer app) { (void)box; (void)child; (void)app; }
gboolean key_pressed(GtkEventControllerKey *controller, guint key, guint code, GdkModifierType mods, gpointer app) {
    (void)controller; (void)key; (void)code; (void)mods; (void)app; return FALSE;
}

static void settle(void) {
    gint64 until = g_get_monotonic_time() + 200000;
    while (g_get_monotonic_time() < until) {
        while (g_main_context_iteration(NULL, FALSE));
        g_usleep(1000);
    }
}
static graphene_rect_t bounds(GtkWidget *widget, GtkWidget *root) {
    graphene_rect_t result;
    g_assert_true(gtk_widget_compute_bounds(widget, root, &result));
    return result;
}
static void same_rect(graphene_rect_t before, graphene_rect_t after) {
    g_assert_cmpfloat_with_epsilon(before.origin.x, after.origin.x, 0.1);
    g_assert_cmpfloat_with_epsilon(before.origin.y, after.origin.y, 0.1);
    g_assert_cmpfloat_with_epsilon(before.size.width, after.size.width, 0.1);
    g_assert_cmpfloat_with_epsilon(before.size.height, after.size.height, 0.1);
}

int main(void) {
    if (!gtk_init_check()) return 77;
    MenuApp app = {0};
    app.application = gtk_application_new("com.anto426.MenuGeometryFixture", G_APPLICATION_NON_UNIQUE);
    g_assert_true(g_application_register(G_APPLICATION(app.application), NULL, NULL));
    menu_build_window(&app);
    gtk_window_unfullscreen(app.window);
    gtk_window_set_default_size(app.window, 1400, 1000);
    gtk_window_present(app.window);
    settle();
    graphene_rect_t panel = bounds(app.panel, GTK_WIDGET(app.window));
    graphene_rect_t sidebar = bounds(app.nav_rail, app.panel);
    GtkWidget *main = gtk_widget_get_last_child(app.panel);
    graphene_rect_t content = bounds(main, app.panel);
    GtkWidget *rail_bounds = g_object_get_data(G_OBJECT(app.deck), "rail-bounds");
    g_assert_cmpint(gtk_widget_get_width(rail_bounds), ==, ANTO_SIZE_SIDEBAR_WIDTH);
    g_assert_cmpint(gtk_widget_get_width(app.deck), ==, ANTO_SIZE_MENU_WIDTH);
    g_assert_cmpint(gtk_widget_get_height(app.deck), ==, ANTO_SIZE_MENU_HEIGHT);

    gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(app.grid), 3);
    gtk_flow_box_set_min_children_per_line(GTK_FLOW_BOX(app.grid), 3);
    for (int i = 0; i < 60; i++) {
        GtkWidget *tile = anto_ui_tile(anto_ui_icon("applications-other-symbolic", 24, NULL),
            "Applicazione con un nome molto lungo da abbreviare", NULL, NULL);
        gtk_flow_box_insert(GTK_FLOW_BOX(app.grid), tile, -1);
        GtkWidget *toggle = anto_ui_toggle_row("Impostazione", "Una descrizione lunga che deve andare a capo nello spazio disponibile", TRUE, NULL);
        gtk_list_box_append(GTK_LIST_BOX(app.list), toggle);
    }
    for (int cycle = 0; cycle < 3; cycle++) {
        for (const char **page = (const char *[]) {"grid", "list", "custom", NULL}; *page; page++) {
            gtk_stack_set_visible_child_name(GTK_STACK(app.content_stack), *page);
            gtk_label_set_text(GTK_LABEL(app.page_title), *page);
            gtk_label_set_text(GTK_LABEL(app.page_subtitle), cycle % 2
                ? "Descrizione estesa della schermata con dati aggiornati" : "");
            gtk_widget_set_visible(app.page_subtitle, cycle % 2);
            settle();
            /* Broadway has no browser to acknowledge later frames. Force the
             * same bounded allocation so hidden/visible stack changes receive
             * real layout rather than comparing the last rendered frame. */
            GtkWidget *panel_bounds = g_object_get_data(G_OBJECT(app.deck), "panel-bounds");
            gtk_widget_queue_allocate(panel_bounds);
            gtk_widget_allocate(panel_bounds, ANTO_SIZE_MENU_WIDTH, ANTO_SIZE_MENU_HEIGHT, -1, NULL);
            same_rect(panel, bounds(app.panel, GTK_WIDGET(app.window)));
            same_rect(sidebar, bounds(app.nav_rail, app.panel));
            same_rect(content, bounds(main, app.panel));
            g_assert_cmpint(gtk_widget_get_width(rail_bounds), ==, ANTO_SIZE_SIDEBAR_WIDTH);
            if (g_str_equal(*page, "list")) {
                GtkAdjustment *scroll = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(app.list_scroll));
                g_assert_cmpfloat(gtk_adjustment_get_upper(scroll), >, gtk_adjustment_get_page_size(scroll));
            }
        }
    }
    gtk_window_destroy(app.window);
    g_ptr_array_free(app.rail_buttons, TRUE);
    g_object_unref(app.application);
    settle();
    g_print("menu geometry: panel, sidebar and content stay fixed across grid/list/custom changes; long lists scroll\n");
    return 0;
}
