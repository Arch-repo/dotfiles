#include "local_config.h"
#include "widgets.h"
#include <glib/gstdio.h>
int main(void) {
  g_autofree char *temporary = g_dir_make_tmp("anto-widget-state-XXXXXX", NULL);
  g_assert_nonnull(temporary);
  g_setenv("ANTO_LOCAL_CONFIG_ROOT", temporary, TRUE);
  g_assert_true(anto_local_config_write_text(
      "widgets/settings.env",
      "export ANTO426_WIDGETS_MASTER_ENABLED=\"1\"\nexport "
      "ANTO426_WIDGETS_ENABLED=\"\"\nexport ANTO426_WIDGETS_LOCKED=\"1\"\n",
      0600, NULL));
  g_assert_true(anto_local_config_write_text(
      "widgets/custom.env",
      "export ANTO426_CUSTOM_WIDGETS=\"scheda_macchina calendario "
      "spettro_audio\"\n",
      0600, NULL));
  const char *legacy =
      "export "
      "LAYOUT_W7363686564615F6D61636368696E61_O6544502D31_X=\"958\"\nexport "
      "LAYOUT_W7363686564615F6D61636368696E61_O6544502D31_Y=\"80\"\nexport "
      "LAYOUT_W7363686564615F6D61636368696E61_O6544502D31_W=\"620\"\nexport "
      "LAYOUT_W7363686564615F6D61636368696E61_O6544502D31_H=\"240\"\n";
  g_assert_true(
      anto_local_config_write_text("widgets/layout.env", legacy, 0600, NULL));
  WidgetSettings s = {0};
  g_assert_true(widget_settings_load(&s, NULL));
  g_assert_true(s.enabled);
  g_assert_true(s.locked);
  int system = widget_kind("system"), agenda = widget_kind("calendar"),
      spectrum = widget_kind("spectrum"), media = widget_kind("media");
  g_assert_true(s.visible[system] && s.visible[agenda] && s.visible[spectrum] &&
                s.visible[media]);
  WidgetGeometry g = widget_geometry(&s, system, "eDP-1", 1600, 1000);
  g_assert_cmpint(g.x, ==, 958);
  g_assert_cmpint(g.y, ==, 80);
  g_assert_cmpint(g.width, ==, 620);
  widget_geometry_save(&s, system, "HDMI-A-1",
                       (WidgetGeometry){30, 40, 500, 300});
  s.visible[agenda] = FALSE;
  g_assert_true(widget_settings_save(&s, NULL));
  widget_settings_clear(&s);
  g_assert_true(widget_settings_load(&s, NULL));
  g_assert_false(s.visible[agenda]);
  g = widget_geometry(&s, system, "HDMI-A-1", 1920, 1080);
  g_assert_cmpint(g.x, ==, 30);
  g_assert_cmpint(g.y, ==, 40);
  g = widget_geometry(&s, system, "eDP-1", 320, 240);
  g_assert_cmpint(g.x + g.width, <=, 320);
  g_assert_cmpint(g.y + g.height, <=, 240);
  g_autofree char *untouched =
      anto_local_config_read_text("widgets/layout.env", NULL, NULL);
  g_assert_cmpstr(untouched, ==, legacy);
  widget_settings_clear(&s);
  g_assert_true(anto_local_config_write_text(
      "widgets/state.json",
      "{\"version\":1,\"enabled\":true,\"widgets\":{\"media\":false,"
      "\"spectrum\":true}}",
      0600, NULL));
  g_assert_true(widget_settings_load(&s, NULL));
  g_assert_true(s.visible[media]);
  g_assert_cmpint(widget_kind("spectrum"), ==, media);
  g_autofree char *migrated =
      anto_local_config_read_text("widgets/state.json", NULL, NULL);
  g_assert_nonnull(strstr(migrated, "\"version\":2"));
  widget_settings_clear(&s);
  g_assert_true(
      anto_local_config_write_text("widgets/state.json", "broken", 0600, NULL));
  g_autoptr(GError) error = NULL;
  g_assert_false(widget_settings_load(&s, &error));
  g_assert_nonnull(error);
  widget_settings_clear(&s);
  g_autofree char *path =
      g_build_filename(temporary, "widgets/state.json", NULL);
  g_unlink(path);
  const char *files[] = {"settings.env", "custom.env", "layout.env",
                         "state.lock"};
  for (guint i = 0; i < G_N_ELEMENTS(files); i++) {
    g_autofree char *file =
        g_build_filename(temporary, "widgets", files[i], NULL);
    g_unlink(file);
  }
  g_autofree char *widgets_dir = g_build_filename(temporary, "widgets", NULL);
  g_rmdir(widgets_dir);
  g_rmdir(temporary);
  g_print("widgets state: legacy data migration, output-local geometry, "
          "bounds, visibility, persistence and malformed state verified\n");
  return 0;
}
