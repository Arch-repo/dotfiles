#include "osd.hpp"
int main(int argc, char **argv) {
  g_setenv("GTK_THEME", "Adwaita:dark", FALSE);
  if (g_getenv("WAYLAND_DISPLAY"))
    g_setenv("GDK_BACKEND", "wayland", TRUE);
  Osd osd;
  auto *application = gtk_application_new("com.anto426.Desktop.Osd",
                                          G_APPLICATION_HANDLES_COMMAND_LINE);
  g_signal_connect(application, "command-line", G_CALLBACK(anto_osd_command),
                   &osd);
  int result = g_application_run(G_APPLICATION(application), argc, argv);
  if (osd.timer)
    g_source_remove(osd.timer);
  g_object_unref(application);
  return result;
}
