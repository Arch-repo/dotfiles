#include "internal.h"
#include "local_config.h"
#include "widgets.h"
#include <json-c/json.h>
static gboolean visible_changed(GtkSwitch *control, gboolean active,
                                gpointer data) {
  MenuApp *app = data;
  const char *id = g_object_get_data(G_OBJECT(control), "widget-id");
  g_autofree char *core = g_build_filename(
      g_get_home_dir(), ".local/libexec/anto426/anto-widgets", NULL);
  const char *argv[] = {core, "set-visible", id, active ? "1" : "0", NULL};
  menu_spawn(app, argv, FALSE);
  return FALSE;
}
static gboolean autostart_changed(GtkSwitch *control, gboolean active,
                                  gpointer data) {
  (void)control;
  MenuApp *app = data;
  g_autofree char *core = g_build_filename(
      g_get_home_dir(), ".local/libexec/anto426/anto-widgets", NULL);
  const char *argv[] = {core, "set-autostart", active ? "1" : "0", NULL};
  menu_spawn(app, argv, FALSE);
  return FALSE;
}
void menu_show_widgets(MenuApp *app) {
  g_autofree char *status = anto_widgets_status();
  gboolean running = g_str_has_prefix(status, "running");
  menu_page_begin(app, "view-grid-symbolic", "Widget",
                  "Le tue schede sul desktop", "Cerca un widget…");
  menu_add_section(app, "Desktop");
  menu_add_shell_item(app,
                      running ? "media-playback-stop-symbolic"
                              : "media-playback-start-symbolic",
                      running ? "Nascondi widget" : "Mostra widget",
                      "Un solo processo nativo per tutti gli schermi", NULL,
                      running ? "$HOME/.config/anto426/widgets.sh stop"
                              : "$HOME/.config/anto426/widgets.sh enable && "
                                "$HOME/.config/anto426/widgets.sh start",
                      TRUE);
  g_autofree char *path = anto_local_config_path("widgets/state.json", NULL);
  json_object *config = path ? json_object_from_file(path) : NULL,
              *items = NULL, *locked = NULL;
  if (config) {
    json_object_object_get_ex(config, "widgets", &items);
    json_object_object_get_ex(config, "locked", &locked);
  }
  gboolean position_locked = !locked || json_object_get_boolean(locked);
  menu_add_shell_item(
      app,
      position_locked ? "changes-allow-symbolic" : "changes-prevent-symbolic",
      position_locked ? "Modifica disposizione" : "Blocca disposizione",
      "Il blocco della posizione lascia attivi i controlli", NULL,
      position_locked ? "$HOME/.config/anto426/widgets.sh unlock"
                      : "$HOME/.config/anto426/widgets.sh lock",
      TRUE);
  menu_add_shell_item(app, "view-restore-symbolic", "Ripristina disposizione",
                      "Ridispone le schede sullo schermo", NULL,
                      "$HOME/.config/anto426/widgets.sh reset-layout", TRUE);
  menu_add_section(app, "Le tue schede");
  for (guint i = 0; i < widget_definition_count; i++) {
    const WidgetDefinition *def = widget_definitions[i];
    json_object *value = NULL;
    gboolean visible = items &&
                       json_object_object_get_ex(items, def->id, &value) &&
                       json_object_get_boolean(value);
    GtkWidget *control = NULL;
    GtkWidget *row =
        anto_ui_toggle_row(def->name, def->detail, visible, &control);
    g_object_set_data_full(G_OBJECT(row), "menu-search",
                           g_utf8_strdown(def->name, -1), g_free);
    g_object_set_data_full(G_OBJECT(control), "widget-id", g_strdup(def->id),
                           g_free);
    g_signal_connect(control, "state-set", G_CALLBACK(visible_changed), app);
    menu_append_widget(app, row);
  }
  json_object *automatic = NULL;
  gboolean autostart =
      !config || !json_object_object_get_ex(config, "autostart", &automatic) ||
      json_object_get_boolean(automatic);
  GtkWidget *automatic_control = NULL;
  GtkWidget *automatic_row =
      anto_ui_toggle_row("Avvio automatico", "Mostra le schede all’accesso",
                         autostart, &automatic_control);
  g_signal_connect(automatic_control, "state-set",
                   G_CALLBACK(autostart_changed), app);
  menu_append_widget(app, automatic_row);
  if (config)
    json_object_put(config);
  menu_set_footer(app, "Controlli sempre interattivi · trascina le "
                       "intestazioni in modalità modifica · Esc chiude");
}
