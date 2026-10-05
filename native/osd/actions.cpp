#include "glass.h"
#include "monitor.h"
#include "palette.h"
#include "primitives.h"
#include <algorithm>
#include <gtk/gtk.h>
#include <gtk4-layer-shell.h>
#include <string>

#include "osd.hpp"
gboolean anto_osd_hide(gpointer data) {
  auto *osd = static_cast<Osd *>(data);
  osd->timer = 0;
  g_application_quit(G_APPLICATION(osd->application));
  return G_SOURCE_REMOVE;
}
int anto_osd_command(GApplication *application, GApplicationCommandLine *line,
                     gpointer data) {
  auto *osd = static_cast<Osd *>(data);
  osd->application = GTK_APPLICATION(application);
  int argc = 0;
  gchar **argv = g_application_command_line_get_arguments(line, &argc);
  std::string kind = argc > 1 ? argv[1] : "volume";
  if (kind == "hide") {
    g_application_quit(application);
    g_strfreev(argv);
    return 0;
  }
  if (kind != "volume" && kind != "mic" && kind != "brightness") {
    g_strfreev(argv);
    return 2;
  }
  int percent = argc > 2 ? std::clamp(std::atoi(argv[2]), 0, 100) : 0;
  bool muted = argc > 3 && std::string(argv[3]) == "1";
  if (!osd->window)
    anto_osd_create(osd);
  if (anto_layer_shell_supported(gdk_display_get_default())) {
    GdkMonitor *monitor = anto_active_monitor(gdk_display_get_default());
    if (monitor) {
      gtk_layer_set_monitor(osd->window, monitor);
      g_object_unref(monitor);
    }
  }
  gtk_label_set_text(GTK_LABEL(osd->title), kind == "brightness" ? "Luminosità"
                                            : kind == "mic"      ? "Microfono"
                                                                 : "Volume");
  std::string value = muted ? "Disattivato" : std::to_string(percent) + "%";
  gtk_label_set_text(GTK_LABEL(osd->value), value.c_str());
  const char *icon = kind == "brightness" ? "display-brightness-symbolic"
                     : kind == "mic"
                         ? (muted ? "microphone-sensitivity-muted-symbolic"
                                  : "audio-input-microphone-symbolic")
                         : (muted ? "audio-volume-muted-symbolic"
                                  : "audio-volume-high-symbolic");
  gtk_image_set_from_icon_name(GTK_IMAGE(osd->icon), icon);
  gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(osd->bar),
                                muted ? 0 : percent / 100.0);
  gtk_window_present(osd->window);
  if (osd->timer)
    g_source_remove(osd->timer);
  osd->timer = g_timeout_add(1400, anto_osd_hide, osd);
  g_strfreev(argv);
  return 0;
}
