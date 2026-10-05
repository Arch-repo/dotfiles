#pragma once
#include <gtk/gtk.h>
G_BEGIN_DECLS
/* The last shell frontend opened owns the session's interactive menu slot. */
void anto_frontend_present(GtkWindow *window);
G_END_DECLS
