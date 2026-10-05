#include "widgets.h"
void widget_label(GtkWidget *label, const char *text) {
  if (g_strcmp0(gtk_label_get_text(GTK_LABEL(label)), text ? text : ""))
    gtk_label_set_text(GTK_LABEL(label), text ? text : "");
}
void widget_open_menu(const char *page) {
  g_autofree char *program =
      g_build_filename(g_get_home_dir(), ".local/bin/anto-menu", NULL);
  g_autoptr(GSubprocess) process =
      g_subprocess_new(G_SUBPROCESS_FLAGS_NONE, NULL, program, page, NULL);
}
static void open_clicked(GtkButton *button, gpointer data) {
  (void)button;
  widget_open_menu(data);
}
GtkWidget *widget_view_new(WidgetStore *s, WidgetKind kind,
                           GtkWidget **header_out) {
  const WidgetDefinition *def = widget_definitions[kind];
  GtkWidget *panel =
      anto_ui_stack(GTK_ORIENTATION_VERTICAL, ANTO_SPACING_MD, "widget-panel");
  gtk_widget_add_css_class(panel, "ui-glass");
  GtkWidget *header = anto_ui_stack(GTK_ORIENTATION_HORIZONTAL, ANTO_SPACING_SM,
                                    "widget-header");
  GtkWidget *icon = anto_ui_icon(def->icon, ANTO_SIZE_ICON, "widget-icon");
  gtk_box_append(GTK_BOX(header), icon);
  GtkWidget *title = anto_ui_text(def->name, "item-title", 1);
  gtk_widget_set_hexpand(title, TRUE);
  gtk_box_append(GTK_BOX(header), title);
  GtkWidget *open = anto_ui_icon_button("go-next-symbolic", "Apri nel menu");
  g_signal_connect(open, "clicked", G_CALLBACK(open_clicked),
                   (gpointer)def->page);
  gtk_box_append(GTK_BOX(header), open);
  gtk_box_append(GTK_BOX(panel), header);
  GtkWidget *body = def->create(s);
  gtk_widget_set_vexpand(body, TRUE);
  gtk_box_append(GTK_BOX(panel), body);
  if (header_out)
    *header_out = header;
  return panel;
}
