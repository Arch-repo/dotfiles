#include "internal.h"
#include "primitives.h"

GtkWidget *anto_calendar_editor_build_editor(CalendarEditor *editor) {
    GtkWidget *root = anto_ui_stack(GTK_ORIENTATION_VERTICAL, ANTO_SPACING_MD, "calendar-editor");
    GtkWidget *intro = anto_ui_card(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_append(GTK_BOX(intro), anto_ui_icon("appointment-new-symbolic", 28, "item-icon"));
    gtk_box_append(GTK_BOX(intro), anto_ui_copy("Un nuovo appuntamento", "L’evento viene salvato sul computer. Puoi aggiornare anche il calendario importato.", NULL, NULL));
    gtk_box_append(GTK_BOX(root), intro);

    GtkWidget *body = anto_ui_stack(GTK_ORIENTATION_HORIZONTAL, ANTO_SPACING_MD, NULL);
    GtkWidget *date = anto_ui_stack(GTK_ORIENTATION_VERTICAL, ANTO_SPACING_SM, "ui-form");
    gtk_box_append(GTK_BOX(date), anto_ui_text("Data", "ui-field-label", 1));
    editor->calendar = GTK_CALENDAR(anto_ui_calendar());
    gtk_widget_add_css_class(GTK_WIDGET(editor->calendar), "month-calendar");
    gtk_calendar_set_show_week_numbers(editor->calendar, FALSE);
    gtk_box_append(GTK_BOX(date), GTK_WIDGET(editor->calendar));
    gtk_widget_set_valign(date, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(body), date);

    GtkWidget *fields = anto_ui_stack(GTK_ORIENTATION_VERTICAL, ANTO_SPACING_MD, NULL);
    gtk_widget_set_hexpand(fields, TRUE);
    gtk_box_append(GTK_BOX(fields), anto_ui_field("Titolo", "Riunione, promemoria, appuntamento…", &editor->title));
    GtkWidget *times = anto_ui_stack(GTK_ORIENTATION_HORIZONTAL, ANTO_SPACING_SM, NULL);
    gtk_box_append(GTK_BOX(times), anto_ui_field("Inizio", "09:00", &editor->start));
    gtk_box_append(GTK_BOX(times), anto_ui_field("Fine", "10:00", &editor->end));
    gtk_editable_set_text(GTK_EDITABLE(editor->start), "09:00");
    gtk_editable_set_text(GTK_EDITABLE(editor->end), "10:00");
    gtk_box_append(GTK_BOX(fields), times);
    GtkWidget *all_day = anto_ui_toggle_row("Tutto il giorno", "Un evento senza un orario specifico", FALSE, &editor->all_day);
    g_signal_connect(editor->all_day, "notify::active", G_CALLBACK(anto_calendar_editor_all_day_changed), editor);
    gtk_box_append(GTK_BOX(fields), all_day);
    gtk_box_append(GTK_BOX(fields), anto_ui_text("Note", "ui-field-label", 1));
    GtkWidget *notes = anto_ui_text_area(&editor->description);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(notes), GTK_WRAP_WORD_CHAR);
    editor->description = gtk_text_view_get_buffer(GTK_TEXT_VIEW(notes));
    GtkWidget *scroll = anto_ui_scroller(notes);
    gtk_widget_set_size_request(scroll, -1, 76);
    gtk_widget_set_vexpand(scroll, FALSE);
    gtk_box_append(GTK_BOX(fields), scroll);
    gtk_box_append(GTK_BOX(body), fields);
    gtk_box_append(GTK_BOX(root), body);
    editor->status = anto_ui_text("", "ui-form-status", 2);
    gtk_box_append(GTK_BOX(root), editor->status);
    GtkWidget *actions = anto_ui_stack(GTK_ORIENTATION_HORIZONTAL, ANTO_SPACING_SM, NULL);
    gtk_widget_set_halign(actions, GTK_ALIGN_END);
    GtkWidget *local = anto_ui_action("Salva evento", "document-save-symbolic", NULL);
    g_signal_connect(local, "clicked", G_CALLBACK(anto_calendar_editor_save_event), editor);
    GtkWidget *sync = anto_ui_action("Salva e aggiorna", "view-refresh-symbolic", "primary");
    g_object_set_data(G_OBJECT(sync), "calendar-sync", GINT_TO_POINTER(TRUE));
    g_signal_connect(sync, "clicked", G_CALLBACK(anto_calendar_editor_save_event), editor);
    gtk_box_append(GTK_BOX(actions), local);
    gtk_box_append(GTK_BOX(actions), sync);
    gtk_box_append(GTK_BOX(root), actions);
    return root;
}

void menu_show_calendar_add(MenuApp *app) {
    menu_page_begin(app, "appointment-new-symbolic", "Nuovo evento", "Scegli la data, il titolo e gli orari", "");
    gtk_widget_set_visible(app->search, FALSE);
    CalendarEditor *editor = g_new0(CalendarEditor, 1);
    editor->app = app;
    GtkWidget *content = anto_calendar_editor_build_editor(editor);
    g_object_set_data_full(G_OBJECT(content), "calendar-editor", editor, g_free);
    menu_set_custom_content(app, anto_ui_scroller(content));
    menu_set_footer(app, "Gli eventi locali restano salvati durante gli aggiornamenti del calendario · Esc chiude");
    gtk_widget_grab_focus(editor->title);
}
