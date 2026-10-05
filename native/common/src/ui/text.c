#include "primitives.h"
#include "palette.h"

GtkWidget *anto_ui_label(const char *text, const char *role) {
    GtkWidget *label = gtk_label_new(text ? text : "");
    gtk_label_set_xalign(GTK_LABEL(label), 0);
    if (role) gtk_widget_add_css_class(label, role);
    return label;
}
GtkWidget *anto_ui_section(const char *title) {
    return anto_ui_text(title, "section-title", 1);
}

GtkWidget *anto_ui_text(const char *text, const char *role, int lines) {
    GtkWidget *label = anto_ui_label(text, role);
    gtk_label_set_max_width_chars(GTK_LABEL(label), 48);
    gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_END);
    gtk_label_set_wrap(GTK_LABEL(label), lines > 1);
    gtk_label_set_wrap_mode(GTK_LABEL(label), PANGO_WRAP_WORD_CHAR);
    gtk_label_set_lines(GTK_LABEL(label), MAX(lines, 1));
    return label;
}

GtkWidget *anto_ui_icon(const char *name, int size, const char *role) {
    GtkWidget *icon = gtk_image_new_from_icon_name(name);
    gtk_image_set_pixel_size(GTK_IMAGE(icon), size);
    if (role) gtk_widget_add_css_class(icon, role);
    return icon;
}

GtkWidget *anto_ui_copy(const char *title, const char *subtitle,
                      GtkWidget **title_out, GtkWidget **detail_out) {
    GtkWidget *copy = anto_ui_stack(GTK_ORIENTATION_VERTICAL, ANTO_SPACING_XS, "ui-copy");
    gtk_widget_set_hexpand(copy, TRUE);
    GtkWidget *heading = anto_ui_text(title, "item-title", 1);
    GtkWidget *detail = anto_ui_text(subtitle, "item-subtitle", 2);
    gtk_box_append(GTK_BOX(copy), heading);
    gtk_box_append(GTK_BOX(copy), detail);
    gtk_widget_set_visible(detail, subtitle && *subtitle);
    if (title_out) *title_out = heading;
    if (detail_out) *detail_out = detail;
    return copy;
}

GtkWidget *anto_ui_heading(const char *icon, const char *title, const char *detail, GtkWidget **icon_out, GtkWidget **title_out, GtkWidget **detail_out) {
    GtkWidget *body = anto_ui_stack(GTK_ORIENTATION_HORIZONTAL, ANTO_SPACING_MD, NULL);
    GtkWidget *image = anto_ui_icon(icon, ANTO_SIZE_ICON_LARGE, "item-icon");
    gtk_box_append(GTK_BOX(body), anto_ui_symbol(image));
    GtkWidget *heading = NULL;
    gtk_box_append(GTK_BOX(body), anto_ui_copy(title, detail, &heading, detail_out));
    gtk_widget_add_css_class(heading, "ui-heading");
    if (icon_out) *icon_out = image;
    if (title_out) *title_out = heading;
    return body;
}
