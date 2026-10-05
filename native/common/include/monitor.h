#pragma once
#include <gtk/gtk.h>
G_BEGIN_DECLS
/* Returns a reference; caller releases it. Pointer output takes precedence. */
GdkMonitor *anto_active_monitor(GdkDisplay *display);
gboolean anto_layer_shell_supported(GdkDisplay *display);
G_END_DECLS
