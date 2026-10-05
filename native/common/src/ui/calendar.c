#include "primitives.h"
#include <time.h>

GtkWidget *anto_ui_calendar(void) {
    GtkWidget *calendar = gtk_calendar_new();
    gtk_widget_add_css_class(calendar, "ui-calendar");
    gtk_calendar_set_show_week_numbers(GTK_CALENDAR(calendar), FALSE);
    gtk_widget_set_halign(calendar, GTK_ALIGN_START);
    gtk_widget_set_valign(calendar, GTK_ALIGN_START);

    /* Keep GTK's date, keyboard and accessibility behavior. Its public child
     * tree is dressed with the same controls and Italian text as the shell. */
    GtkWidget *header = gtk_widget_get_first_child(calendar);
    const char *tips[] = {"Mese precedente", "Mese successivo", "Anno precedente", "Anno successivo"};
    const char *months[] = {"Gennaio", "Febbraio", "Marzo", "Aprile", "Maggio", "Giugno",
                            "Luglio", "Agosto", "Settembre", "Ottobre", "Novembre", "Dicembre"};
    guint arrow = 0;
    for (GtkWidget *child = gtk_widget_get_first_child(header); child;
         child = gtk_widget_get_next_sibling(child)) {
        if (GTK_IS_BUTTON(child)) {
            gtk_widget_add_css_class(child, "round-button");
            if (arrow < G_N_ELEMENTS(tips)) gtk_widget_set_tooltip_text(child, tips[arrow++]);
        } else if (GTK_IS_STACK(child)) {
            g_autoptr(GtkSelectionModel) pages = gtk_stack_get_pages(GTK_STACK(child));
            for (guint i = 0; i < MIN(12u, g_list_model_get_n_items(G_LIST_MODEL(pages))); i++) {
                g_autoptr(GtkStackPage) page = g_list_model_get_item(G_LIST_MODEL(pages), i);
                GtkWidget *label = gtk_stack_page_get_child(page);
                if (GTK_IS_LABEL(label)) gtk_label_set_text(GTK_LABEL(label), months[i]);
            }
        }
    }
    GtkWidget *grid = gtk_widget_get_next_sibling(header);
    const char *days[] = {"Dom", "Lun", "Mar", "Mer", "Gio", "Ven", "Sab"};
    for (GtkWidget *label = gtk_widget_get_first_child(grid); label;
         label = gtk_widget_get_next_sibling(label)) {
        if (gtk_widget_has_css_class(label, "day-number")) {
            gtk_widget_set_halign(label, GTK_ALIGN_CENTER);
            gtk_widget_set_valign(label, GTK_ALIGN_CENTER);
        } else if (GTK_IS_LABEL(label) && gtk_widget_has_css_class(label, "day-name")) {
            for (int i = 0; i < 7; i++) {
                struct tm weekday = {.tm_wday = i};
                char name[64];
                if (strftime(name, sizeof(name), "%a", &weekday) &&
                    g_strcmp0(name, gtk_label_get_text(GTK_LABEL(label))) == 0) {
                    gtk_label_set_text(GTK_LABEL(label), days[i]);
                    break;
                }
            }
        }
    }
    return calendar;
}
