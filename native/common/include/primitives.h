#pragma once
#include <gtk/gtk.h>
#include "design_tokens.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Reusable visual primitives; domain actions remain in each page. */
void anto_ui_init(GdkDisplay *display);
GtkWidget *anto_ui_label(const char *text, const char *role);
GtkWidget *anto_ui_stack(GtkOrientation orientation, int spacing, const char *role);
GtkWidget *anto_ui_icon_button(const char *icon, const char *tooltip);
GtkWidget *anto_ui_text(const char *text, const char *role, int lines);
GtkWidget *anto_ui_icon(const char *name, int size, const char *role);
GtkWidget *anto_ui_copy(const char *title, const char *subtitle, GtkWidget **title_out, GtkWidget **detail_out);
GtkWidget *anto_ui_action(const char *label, const char *icon, const char *role);
GtkWidget *anto_ui_metric(const char *caption, GtkWidget **value_out);
GtkWidget *anto_ui_scroller(GtkWidget *child);
GtkWidget *anto_ui_field(const char *label, const char *placeholder, GtkWidget **entry_out);
GtkWidget *anto_ui_toggle_row(const char *title, const char *subtitle, gboolean active, GtkWidget **control_out);
GtkWidget *anto_ui_search(const char *placeholder);
GtkWidget *anto_ui_choice(const char *label);
GtkWidget *anto_ui_password(const char *label);
GtkWidget *anto_ui_switch(gboolean active, const char *label);
GtkWidget *anto_ui_scale(double value, double min, double max, double step, const char *label);
GtkWidget *anto_ui_progress(void);
GtkWidget *anto_ui_level(double min, double max);
GtkWidget *anto_ui_action_label(GtkWidget *button);
GtkWidget *anto_ui_action_icon(GtkWidget *button);
void anto_ui_action_set_text(GtkWidget *button, const char *text);
GtkWidget *anto_ui_card(GtkOrientation orientation);
GtkWidget *anto_ui_row(GtkWidget *leading, const char *title, const char *detail, GtkWidget **title_out, GtkWidget **detail_out);
GtkWidget *anto_ui_symbol(GtkWidget *icon);
GtkWidget *anto_ui_badge(const char *text);
GtkWidget *anto_ui_indicator(const char *text, GtkWidget **dot_out, GtkWidget **label_out);
GtkWidget *anto_ui_empty(const char *icon, const char *title, const char *detail, gboolean loading, GtkWidget **lead_out, GtkWidget **title_out, GtkWidget **detail_out);
GtkWidget *anto_ui_heading(const char *icon, const char *title, const char *detail, GtkWidget **icon_out, GtkWidget **title_out, GtkWidget **detail_out);
GtkWidget *anto_ui_slider_card(const char *icon, const char *title, const char *detail, double value, double min, double max, double step, GtkWidget **icon_out, GtkWidget **detail_out, GtkWidget **scale_out);
GtkWidget *anto_ui_tile(GtkWidget *icon, const char *title, const char *detail, const char *badge);
GtkWidget *anto_ui_glyph_tile(const char *text, GtkWidget **glyph_out);
GtkWidget *anto_ui_picture(const char *filename);
GtkWidget *anto_ui_text_area(GtkTextBuffer **buffer_out);
GtkWidget *anto_ui_calendar(void);
GtkWidget *anto_ui_section(const char *title);
GtkWidget *anto_ui_spinner(void);
#ifdef __cplusplus
}
#endif
