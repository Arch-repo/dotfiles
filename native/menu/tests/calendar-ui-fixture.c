#include "../src/modules/calendar/internal.h"
#include "primitives.h"
#include "palette.h"
#include <glib/gstdio.h>

/* Minimal host around the production calendar. All data and CSS writes stay
 * in the private XDG directories created by gtk-isolated.py. */
void menu_add_item(MenuApp *app, const char *icon, const char *title, const char *detail,
                   const char *badge, MenuAction action, gpointer data, GDestroyNotify destroy) {
    (void)icon; (void)action; (void)data; (void)destroy;
    GtkWidget *row = anto_ui_row(NULL, title, detail, NULL, NULL);
    GtkWidget *status = anto_ui_badge(badge);
    gtk_widget_add_css_class(status, "item-badge");
    gtk_box_append(GTK_BOX(row), status);
    gtk_list_box_append(GTK_LIST_BOX(app->list), row);
}

static void settle(void) {
    gint64 until = g_get_monotonic_time() + 180000;
    while (g_get_monotonic_time() < until) {
        while (g_main_context_iteration(NULL, FALSE));
        g_usleep(1000);
    }
}

static void palette(const char *accent, gboolean complete) {
    g_autofree char *directory = g_build_filename(g_get_user_config_dir(), "anto426-local/theme", NULL);
    g_mkdir_with_parents(directory, 0700);
    g_autofree char *path = g_build_filename(directory, "colors.css", NULL);
    g_autofree char *temp = g_strconcat(path, ".next", NULL);
    g_autofree char *css = complete ? g_strdup_printf(
        "@define-color background #222222;\n@define-color surface #444444;\n"
        "@define-color base #252525;\n@define-color base-alt #333333;\n"
        "@define-color foreground #ffffff;\n@define-color accent %s;\n"
        "@define-color selected-fg #111111;\n@define-color border #555555;\n"
        "@define-color muted #aaaaaa;\n@define-color red #ff7788;\n"
        "@define-color yellow #ffd777;\n@define-color green #88eeaa;\n", accent) : g_strdup("@define-color accent #0000ff;\n");
    g_assert_true(g_file_set_contents(temp, css, -1, NULL));
    g_assert_cmpint(g_rename(temp, path), ==, 0);
    settle();
}

static void colour(GtkWidget *widget, const char *hex) {
    GdkRGBA expected, actual;
    gdk_rgba_parse(&expected, hex);
    gtk_widget_get_color(widget, &actual);
    g_assert_cmpfloat_with_epsilon(actual.red, expected.red, .001);
    g_assert_cmpfloat_with_epsilon(actual.green, expected.green, .001);
    g_assert_cmpfloat_with_epsilon(actual.blue, expected.blue, .001);
}

static GtkWidget *day(GtkCalendar *calendar, int value) {
    GtkWidget *header = gtk_widget_get_first_child(GTK_WIDGET(calendar));
    GtkWidget *grid = gtk_widget_get_next_sibling(header);
    for (GtkWidget *label = gtk_widget_get_first_child(grid); label;
         label = gtk_widget_get_next_sibling(label)) {
        if (!GTK_IS_LABEL(label) || !gtk_widget_has_css_class(label, "day-number") ||
            gtk_widget_has_css_class(label, "other-month")) continue;
        if (atoi(gtk_label_get_text(GTK_LABEL(label))) == value) return label;
    }
    g_assert_not_reached();
}

static void choose(CalendarView *view, int selected, guint count) {
    g_autoptr(GDateTime) date = g_date_time_new_local(2026, 10, selected, 12, 0, 0);
    gtk_calendar_set_date(view->calendar, date);
    settle();
    g_assert_cmpuint(view->day_rows->len, ==, count);
    g_assert_cmpint(gtk_widget_get_visible(view->day_empty), ==, count == 0);
    g_autofree char *total = g_strdup_printf("%u", count);
    g_assert_cmpstr(gtk_label_get_text(GTK_LABEL(view->selected_count)), ==, total);
}

int main(int argc, char **argv) {
    if (!gtk_init_check()) return 77;
    GtkCssProvider *hostile = gtk_css_provider_new();
    gtk_css_provider_load_from_string(hostile,
        ".calendar-test-root { background: #222222; }"
        "calendar.view { border: 2px solid black; } calendar.view > header { border: 2px solid black; }"
        "calendar.view > grid label:selected { background: blue; color: white; }");
    gtk_style_context_add_provider_for_display(gdk_display_get_default(), GTK_STYLE_PROVIDER(hostile),
                                               GTK_STYLE_PROVIDER_PRIORITY_THEME + 1);
    g_object_unref(hostile);
    for (int i = 1; i < argc; i++) {
        GtkCssProvider *style = gtk_css_provider_new();
        gtk_css_provider_load_from_path(style, argv[i]);
        gtk_style_context_add_provider_for_display(gdk_display_get_default(), GTK_STYLE_PROVIDER(style),
                                                   GTK_STYLE_PROVIDER_PRIORITY_USER + i);
        g_object_unref(style);
    }
    palette("#ee77bb", TRUE);
    anto_watch_palette(gdk_display_get_default());
    g_autofree char *directory = g_build_filename(g_get_user_data_dir(), "anto426/calendar", NULL);
    g_mkdir_with_parents(directory, 0700);
    g_autofree char *events = g_build_filename(directory, "events.json", NULL);
    g_assert_true(g_file_set_contents(events,
        "[{\"id\":\"a\",\"date\":\"2026-10-11\",\"title\":\"Intera giornata\",\"all_day\":true},"
        "{\"id\":\"b\",\"date\":\"2026-10-11\",\"title\":\"Primo\",\"start\":\"09:00\",\"end\":\"10:00\"},"
        "{\"id\":\"c\",\"date\":\"2026-10-11\",\"title\":\"Secondo\",\"start\":\"12:00\"},"
        "{\"id\":\"d\",\"date\":\"2026-10-11\",\"title\":\"Terzo\",\"start\":\"14:00\"},"
        "{\"id\":\"e\",\"date\":\"2026-10-11\",\"title\":\"Quinto visibile\",\"start\":\"16:00\"},"
        "{\"id\":\"f\",\"date\":\"2026-10-06\",\"title\":\"Altro giorno\"}]", -1, NULL));
    MenuApp app = {0};
    app.window = GTK_WINDOW(gtk_window_new());
    app.current_page = "calendar";
    app.list = gtk_list_box_new();
    app.list_scroll = anto_ui_scroller(app.list);
    app.page_subtitle = anto_ui_text("", "item-subtitle", 1);
    CalendarView *view = g_new0(CalendarView, 1);
    view->app = &app;
    g_weak_ref_init(&view->window, G_OBJECT(app.window));
    view->events = anto_calendar_read_events();
    view->agenda_rows = g_ptr_array_new_with_free_func(anto_calendar_agenda_row_free);
    g_object_set_data(G_OBJECT(app.window), CALENDAR_VIEW_KEY, view);
    g_strlcpy(anto_calendar_selected_calendar_date, "2026-10-11", 11);
    GtkWidget *root = anto_ui_stack(GTK_ORIENTATION_VERTICAL, 8, NULL);
    gtk_widget_add_css_class(root, "calendar-test-root");
    gtk_box_append(GTK_BOX(root), anto_calendar_build_month_card(view));
    gtk_box_append(GTK_BOX(root), app.page_subtitle);
    gtk_box_append(GTK_BOX(root), app.list_scroll);
    gtk_window_set_child(app.window, root);
    gtk_window_set_default_size(app.window, 900, 550);
    gtk_window_present(app.window);
    settle();
    choose(view, 11, 5);
    CalendarAgendaRow *last = g_ptr_array_index(view->day_rows, 4);
    g_assert_cmpstr(gtk_label_get_text(GTK_LABEL(last->title)), ==, "Quinto visibile");
    CalendarAgendaRow *timed = g_ptr_array_index(view->day_rows, 1);
    g_assert_nonnull(strstr(gtk_label_get_text(GTK_LABEL(timed->subtitle)), "09:00–10:00"));
    graphene_rect_t month, details;
    g_assert_true(gtk_widget_compute_bounds(GTK_WIDGET(view->calendar), root, &month));
    g_assert_true(gtk_widget_compute_bounds(view->selected_events, root, &details));
    g_assert_cmpfloat(details.origin.x, >, month.origin.x + month.size.width);
    g_assert_cmpint(gtk_widget_get_width(day(view->calendar, 11)), <=, 40);
    colour(day(view->calendar, 6), "#ee77bb");
    colour(day(view->calendar, 11), "#111111");
    const char *image = g_getenv("ANTO426_CALENDAR_FIXTURE_IMAGE");
    if (image) {
        g_autoptr(GdkPaintable) paintable = gtk_widget_paintable_new(root);
        GtkSnapshot *snapshot = gtk_snapshot_new();
        gdk_paintable_snapshot(paintable, snapshot, gtk_widget_get_width(root), gtk_widget_get_height(root));
        g_autoptr(GskRenderNode) node = gtk_snapshot_free_to_node(snapshot);
        g_autoptr(GdkTexture) texture = gsk_renderer_render_texture(gtk_native_get_renderer(GTK_NATIVE(app.window)), node, NULL);
        g_assert_true(gdk_texture_save_to_png(texture, image));
    }
    choose(view, 12, 0);
    choose(view, 6, 1);
    choose(view, 11, 5);
    GtkWidget *retained = ((CalendarAgendaRow *)g_ptr_array_index(view->day_rows, 0))->row;
    anto_calendar_update_selection(view);
    g_assert_true(((CalendarAgendaRow *)g_ptr_array_index(view->day_rows, 0))->row == retained);
    palette("#66ccaa", TRUE);
    colour(day(view->calendar, 6), "#66ccaa");
    palette("#0000ff", FALSE);
    colour(day(view->calendar, 6), "#66ccaa");
    g_assert_true(g_file_set_contents(events,
        "[{\"date\":\"2026-10-11\",\"title\":\"Aggiornato in diretta\",\"all_day\":true}]", -1, NULL));
    menu_calendar_live_event(&app);
    settle();
    g_assert_cmpuint(view->day_rows->len, ==, 1);
    CalendarAgendaRow *updated = g_ptr_array_index(view->day_rows, 0);
    g_assert_cmpstr(gtk_label_get_text(GTK_LABEL(updated->title)), ==, "Aggiornato in diretta");
    gtk_window_destroy(app.window);
    g_print("calendar UI: right pane, all events, date changes, live data, row reuse and palette reload verified\n");
    return 0;
}
