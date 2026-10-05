#include "menu.h"

#include <stdlib.h>
#include <string.h>

typedef void (*PageFunction)(MenuApp *app);

typedef struct {
    const char *name;
    PageFunction show;
} Page;

static const Page pages[] = {
    {"apps", menu_show_launcher},
    {"system", menu_show_system},
    {"audio", menu_show_audio},
    {"wifi", menu_show_wifi},
    {"bluetooth", menu_show_bluetooth},
    {"brightness", menu_show_brightness},
    {"display", menu_show_display},
    {"capture", menu_show_capture},
    {"record", menu_show_record},
    {"power", menu_show_power},
    {"clipboard", menu_show_clipboard},
    {"emoji", menu_show_emoji},
    {"hardware", menu_show_hardware},
    {"notifications", menu_show_notifications},
    {"calendar", menu_show_calendar},
    {"calendar-add", menu_show_calendar_add},
    {"keyboard", menu_show_keyboard},
    {"widgets", menu_show_widgets},
    {"floating", menu_show_floating},
    {"wallpaper", menu_show_wallpaper},
    {"background", menu_show_background},
    {"shortcuts", menu_show_shortcuts},
    {"settings", menu_show_settings},
};

static PageFunction page_lookup(const char *name) {
    for (guint i = 0; i < G_N_ELEMENTS(pages); i++)
        if (g_strcmp0(name, pages[i].name) == 0) return pages[i].show;
    return menu_show_launcher;
}

gboolean menu_known_page(const char *name) {
    for (guint i = 0; i < G_N_ELEMENTS(pages); i++)
        if (g_strcmp0(name, pages[i].name) == 0) return TRUE;
    return FALSE;
}
void menu_print_pages(void) {
    for (guint i = 0; i < G_N_ELEMENTS(pages); i++) g_print("%s\n",pages[i].name);
}

static void reset_visible_scroll(MenuApp *app) {
    GtkAdjustment *adjustment = NULL;
    if (app->layout == MENU_LAYOUT_GRID) {
        adjustment = gtk_scrolled_window_get_vadjustment(
            GTK_SCROLLED_WINDOW(app->grid_scroll));
    } else if (app->layout == MENU_LAYOUT_LIST) {
        adjustment = gtk_scrolled_window_get_vadjustment(
            GTK_SCROLLED_WINDOW(app->list_scroll));
    } else {
        GtkWidget *child = gtk_widget_get_first_child(app->custom_holder);
        if (GTK_IS_SCROLLED_WINDOW(child))
            adjustment = gtk_scrolled_window_get_vadjustment(
                GTK_SCROLLED_WINDOW(child));
    }
    if (adjustment)
        gtk_adjustment_set_value(adjustment,
                                 gtk_adjustment_get_lower(adjustment));
}

static gboolean reset_visible_scroll_idle(gpointer data) {
    reset_visible_scroll(data);
    return G_SOURCE_REMOVE;
}

static void show_page(MenuApp *app, const char *page) {
    g_free(app->current_page);
    app->current_page = g_strdup(page && *page ? page : "apps");
    page_lookup(app->current_page)(app);
    if (app->layout == MENU_LAYOUT_GRID) {
        GtkFlowBoxChild *first = gtk_flow_box_get_child_at_index(GTK_FLOW_BOX(app->grid), 0);
        if (first) gtk_flow_box_select_child(GTK_FLOW_BOX(app->grid), first);
    } else {
        GtkListBoxRow *first = gtk_list_box_get_row_at_index(GTK_LIST_BOX(app->list), 0);
        while (first && !gtk_list_box_row_get_selectable(first))
            first = gtk_list_box_get_row_at_index(GTK_LIST_BOX(app->list), gtk_list_box_row_get_index(first) + 1);
        if (first) gtk_list_box_select_row(GTK_LIST_BOX(app->list), first);
    }
    if (gtk_widget_get_visible(app->search))
        gtk_widget_grab_focus(app->search);
    /* Selecting the first actionable row can ask GtkScrolledWindow to reveal
     * it, skipping large informational widgets placed before it (calendar,
     * monitor arranger, Bluetooth hero).  Restore the top after allocation so
     * every page consistently opens at its real beginning. */
    reset_visible_scroll(app);
    g_idle_add(reset_visible_scroll_idle, app);
}

void menu_open(MenuApp *app, const char *page) {
    if (!app || !page || !*page) return;
    if (app->current_page && g_strcmp0(app->current_page, page) != 0)
        g_ptr_array_add(app->history, g_strdup(app->current_page));
    show_page(app, page);
}

void menu_back(MenuApp *app) {
    if (!app || !app->history || app->history->len == 0) {
        menu_close(app);
        return;
    }
    guint index = app->history->len - 1;
    char *page = g_ptr_array_steal_index(app->history, index);
    show_page(app, page);
    g_free(page);
}

void menu_spawn(MenuApp *app, const char *const argv[], gboolean close_after) {
    g_autoptr(GError) error = NULL;
    g_autoptr(GSubprocessLauncher) launcher = g_subprocess_launcher_new(G_SUBPROCESS_FLAGS_NONE);
    /* GUI tools and applications reached through helpers use the desktop theme. */
    g_subprocess_launcher_unsetenv(launcher, "GTK_THEME");
    GSubprocess *process = g_subprocess_launcher_spawnv(launcher, argv, &error);
    if (!process) {
        menu_notify("Menu", error ? error->message : "Impossibile avviare il comando");
        return;
    }
    g_object_unref(process);
    if (close_after) menu_close(app);
}

void menu_spawn_shell(MenuApp *app, const char *command, gboolean close_after) {
    const char *argv[] = {"/bin/sh", "-lc", command, NULL};
    menu_spawn(app, argv, close_after);
}

char *menu_capture(const char *command) {
    char *stdout_text = NULL;
    char *stderr_text = NULL;
    int status = 0;
    g_autoptr(GError) error = NULL;
    char *argv[] = {"/bin/sh", "-lc", (char *)command, NULL};
    if (!g_spawn_sync(NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL,
                      &stdout_text, &stderr_text, &status, &error)) {
        g_free(stderr_text);
        return g_strdup("");
    }
    g_free(stderr_text);
    if (!stdout_text) return g_strdup("");
    g_strstrip(stdout_text);
    return stdout_text;
}

gboolean menu_run_with_input(const char *const argv[], const char *input,
                             char **stdout_text, char **stderr_text) {
    g_autoptr(GError) error = NULL;
    GSubprocess *process = g_subprocess_newv(
        argv, G_SUBPROCESS_FLAGS_STDIN_PIPE | G_SUBPROCESS_FLAGS_STDOUT_PIPE |
                  G_SUBPROCESS_FLAGS_STDERR_PIPE, &error);
    if (!process) return FALSE;
    gboolean ok = g_subprocess_communicate_utf8(process, input, NULL,
                                                 stdout_text, stderr_text, &error);
    ok = ok && g_subprocess_get_successful(process);
    g_object_unref(process);
    return ok;
}

void menu_notify(const char *title, const char *body) {
    const char *argv[] = {"notify-send", title ? title : "Menu", body ? body : "", NULL};
    g_autoptr(GError) error = NULL;
    GSubprocess *process = g_subprocess_newv(argv, G_SUBPROCESS_FLAGS_NONE, &error);
    if (process) g_object_unref(process);
}

void menu_open_terminal(MenuApp *app, const char *title, const char *command) {
    g_autofree char *quoted_title = g_shell_quote(title ? title : "Terminale");
    g_autofree char *quoted_command = g_shell_quote(command);
    g_autofree char *shell = g_strdup_printf(
        "if command -v ghostty >/dev/null; then exec ghostty --title=%s -e sh -lc %s; "
        "elif command -v foot >/dev/null; then exec foot -T %s sh -lc %s; "
        "else exec xterm -T %s -e sh -lc %s; fi",
        quoted_title, quoted_command, quoted_title, quoted_command, quoted_title, quoted_command);
    menu_spawn_shell(app, shell, TRUE);
}

char *menu_config_path(const char *suffix) {
    const char *base = g_get_user_config_dir();
    return g_build_filename(base, suffix, NULL);
}

char *menu_home_path(const char *suffix) {
    return g_build_filename(g_get_home_dir(), suffix, NULL);
}
