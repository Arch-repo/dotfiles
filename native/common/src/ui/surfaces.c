#include "primitives.h"
#include "palette.h"

GtkWidget *anto_ui_stack(GtkOrientation orientation, int spacing, const char *role) {
    GtkWidget *stack = gtk_box_new(orientation, spacing);
    if (role) gtk_widget_add_css_class(stack, role);
    return stack;
}

GtkWidget *anto_ui_metric(const char *caption, GtkWidget **value_out) {
    GtkWidget *metric = anto_ui_stack(GTK_ORIENTATION_VERTICAL, ANTO_SPACING_XS, "ui-metric");
    GtkWidget *value = anto_ui_text("—", "ui-metric-value", 1);
    gtk_box_append(GTK_BOX(metric), value);
    gtk_box_append(GTK_BOX(metric), anto_ui_text(caption, "ui-caption", 1));
    if (value_out) *value_out = value;
    return metric;
}

GtkWidget *anto_ui_scroller(GtkWidget *child) {
    GtkWidget *scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_propagate_natural_width(GTK_SCROLLED_WINDOW(scroll), FALSE);
    gtk_scrolled_window_set_propagate_natural_height(GTK_SCROLLED_WINDOW(scroll), FALSE);
    gtk_widget_set_size_request(scroll, -1, 0);
    gtk_widget_set_vexpand(scroll, TRUE);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), child);
    return scroll;
}

GtkWidget *anto_ui_card(GtkOrientation orientation) {
    return anto_ui_stack(orientation, ANTO_SPACING_MD, "ui-card");
}

GtkWidget *anto_ui_row(GtkWidget *leading, const char *title, const char *detail, GtkWidget **title_out, GtkWidget **detail_out) {
    GtkWidget *row = anto_ui_stack(GTK_ORIENTATION_HORIZONTAL, ANTO_SPACING_MD, "ui-row");
    if (leading) gtk_box_append(GTK_BOX(row), anto_ui_symbol(leading));
    gtk_box_append(GTK_BOX(row), anto_ui_copy(title, detail, title_out, detail_out));
    if (detail_out) gtk_widget_set_visible(*detail_out, TRUE);
    return row;
}

GtkWidget *anto_ui_symbol(GtkWidget *icon) {
    GtkWidget *symbol = anto_ui_stack(GTK_ORIENTATION_HORIZONTAL, 0, "ui-symbol");
    gtk_widget_set_valign(symbol, GTK_ALIGN_CENTER);
    gtk_widget_set_halign(symbol, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(symbol), icon);
    return symbol;
}

GtkWidget *anto_ui_badge(const char *text) { return anto_ui_text(text, "ui-status", 1); }

GtkWidget *anto_ui_indicator(const char *text, GtkWidget **dot_out, GtkWidget **label_out) {
    GtkWidget *body = anto_ui_stack(GTK_ORIENTATION_HORIZONTAL, ANTO_SPACING_SM, "ui-status");
    GtkWidget *dot = anto_ui_stack(GTK_ORIENTATION_HORIZONTAL, 0, "ui-status-dot");
    gtk_widget_set_valign(dot, GTK_ALIGN_CENTER);
    GtkWidget *label = anto_ui_text(text, "ui-status-label", 1);
    gtk_box_append(GTK_BOX(body), dot);
    gtk_box_append(GTK_BOX(body), label);
    if (dot_out) *dot_out = dot;
    if (label_out) *label_out = label;
    return body;
}

GtkWidget *anto_ui_empty(const char *icon, const char *title, const char *detail, gboolean loading, GtkWidget **lead_out, GtkWidget **title_out, GtkWidget **detail_out) {
    GtkWidget *lead;
    if (loading) {
        lead = anto_ui_spinner();
    } else lead = anto_ui_icon(icon, ANTO_SIZE_ICON_LARGE, "item-icon");
    GtkWidget *row = anto_ui_row(lead, title, detail, title_out, detail_out);
    gtk_widget_add_css_class(row, "ui-empty");
    if (lead_out) *lead_out = lead;
    return row;
}

GtkWidget *anto_ui_slider_card(const char *icon, const char *title, const char *detail, double value, double min, double max, double step, GtkWidget **icon_out, GtkWidget **detail_out, GtkWidget **scale_out) {
    GtkWidget *card = anto_ui_card(GTK_ORIENTATION_VERTICAL);
    GtkWidget *image = anto_ui_icon(icon, ANTO_CONTROL_ROW_ICON, "item-icon");
    GtkWidget *heading = anto_ui_row(image, title, detail, NULL, detail_out);
    gtk_widget_add_css_class(heading, "ui-flat");
    gtk_box_append(GTK_BOX(card), heading);
    GtkWidget *scale = anto_ui_scale(value, min, max, step, title);
    gtk_box_append(GTK_BOX(card), scale);
    if (icon_out) *icon_out = image;
    if (scale_out) *scale_out = scale;
    return card;
}

GtkWidget *anto_ui_tile(GtkWidget *icon, const char *title, const char *detail, const char *badge) {
    GtkWidget *card = anto_ui_card(GTK_ORIENTATION_VERTICAL);
    gtk_box_set_spacing(GTK_BOX(card), ANTO_SPACING_SM);
    gtk_widget_add_css_class(card, "ui-tile");
    gtk_box_append(GTK_BOX(card), anto_ui_symbol(icon));
    gtk_box_append(GTK_BOX(card), anto_ui_copy(title, detail, NULL, NULL));
    if (badge && *badge) {
        GtkWidget *status = anto_ui_badge(badge);
        gtk_widget_set_halign(status, GTK_ALIGN_START);
        gtk_widget_add_css_class(status, "tile-badge");
        gtk_box_append(GTK_BOX(card), status);
    }
    return card;
}

GtkWidget *anto_ui_picture(const char *filename) {
    GtkWidget *picture = gtk_picture_new_for_filename(filename);
    gtk_picture_set_can_shrink(GTK_PICTURE(picture), TRUE);
    gtk_picture_set_content_fit(GTK_PICTURE(picture), GTK_CONTENT_FIT_COVER);
    gtk_widget_add_css_class(picture, "ui-media");
    return picture;
}

GtkWidget *anto_ui_glyph_tile(const char *text, GtkWidget **glyph_out) {
    GtkWidget *tile = anto_ui_card(GTK_ORIENTATION_VERTICAL);
    gtk_widget_add_css_class(tile, "ui-glyph-tile");
    gtk_widget_set_hexpand(tile, TRUE);
    GtkWidget *glyph = anto_ui_text(text, "ui-glyph", 1);
    gtk_widget_set_halign(glyph, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(glyph, GTK_ALIGN_CENTER);
    gtk_widget_set_vexpand(glyph, TRUE);
    gtk_box_append(GTK_BOX(tile), glyph);
    if (glyph_out) *glyph_out = glyph;
    return tile;
}
