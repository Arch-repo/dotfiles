#include "internal.h"
#include "primitives.h"

GtkWidget *anto_hardware_add_metric(MenuApp *app, const char *icon,
                                      const char *title, const char *value,
                                      const char *badge,
                                      GtkWidget **detail_out) {
    menu_add_item(app, icon, title, value, badge, NULL, NULL, NULL);
    GtkWidget *row = gtk_widget_get_last_child(app->list);
    *detail_out = anto_hardware_find_css_descendant(row, "item-subtitle");
    return row;
}

gboolean anto_hardware_row_search_set(GtkWidget *row, const char *title,
                                        const char *subtitle,
                                        const char *badge) {
    if (!row) return FALSE;
    g_autofree char *combined = g_strdup_printf(
        "%s %s %s", title ? title : "", subtitle ? subtitle : "",
        badge ? badge : "");
    g_autofree char *search = g_utf8_strdown(combined, -1);
    const char *current =
        g_object_get_data(G_OBJECT(row), "menu-search");
    if (g_strcmp0(current, search) == 0) return FALSE;
    g_object_set_data_full(G_OBJECT(row), "menu-search",
                           g_steal_pointer(&search), g_free);
    return TRUE;
}

GtkWidget *anto_hardware_add_profile(HardwareRuntime *runtime,
                                       const char *icon, const char *title,
                                       const char *subtitle,
                                       const char *profile,
                                       GtkWidget **badge_out) {
    HardwareProfileAction *action = g_new0(HardwareProfileAction, 1);
    action->runtime = runtime;
    action->profile = g_strdup(profile);
    menu_add_item(runtime->app, icon, title, subtitle, NULL,
                  anto_hardware_profile_action, action,
                  anto_hardware_profile_action_free);
    GtkWidget *row = gtk_widget_get_last_child(runtime->app->list);
    GtkWidget *content =
        gtk_list_box_row_get_child(GTK_LIST_BOX_ROW(row));
    GtkWidget *badge = anto_ui_badge("");
    gtk_widget_add_css_class(badge, "item-badge");
    gtk_widget_set_valign(badge, GTK_ALIGN_CENTER);
    gtk_widget_set_visible(badge, FALSE);
    gtk_box_append(GTK_BOX(content), badge);
    *badge_out = badge;
    return row;
}

void anto_hardware_render(HardwareRuntime *runtime,
                            const HardwareSnapshot *snapshot) {
    const HardwareSnapshot empty = {0};
    if (!snapshot) snapshot = &empty;
    MenuApp *app = runtime->app;
    memset(runtime->metric_rows, 0, sizeof(runtime->metric_rows));
    memset(runtime->profile_rows, 0, sizeof(runtime->profile_rows));
    menu_page_begin(app, "computer-symbolic", "Hardware",
                    "Metriche raccolte in background",
                    "Cerca una risorsa o uno strumento…");
    menu_add_section(app, "STATO LIVE");
    GtkWidget *root = runtime->metric_rows[0] = anto_hardware_add_metric(
        app, "cpu-symbolic", "Carico CPU",
        anto_hardware_present(snapshot->cpu, "n/d"), "LIVE", &runtime->cpu_detail);
    runtime->metric_rows[1] = anto_hardware_add_metric(
        app, "drive-multidisk-symbolic", "Memoria",
        anto_hardware_present(snapshot->memory, "n/d"), NULL,
        &runtime->memory_detail);
    runtime->metric_rows[2] = anto_hardware_add_metric(
        app, "drive-harddisk-symbolic", "Disco di sistema",
        anto_hardware_present(snapshot->disk, "n/d"), NULL, &runtime->disk_detail);
    runtime->metric_rows[3] = anto_hardware_add_metric(
        app, "temperature-symbolic", "Temperatura",
        anto_hardware_present(snapshot->temperature, "Sensore non esposto"), NULL,
        &runtime->temperature_detail);

    menu_add_section(app, "STRUMENTI");
    menu_add_shell_item(app, "utilities-system-monitor-symbolic", "Btop",
                        "Processi, CPU, RAM, disco e rete", NULL,
                        "ghostty -e btop", TRUE);
    menu_add_shell_item(app, "utilities-system-monitor-symbolic",
                        "Sensori live",
                        "Temperature e dati hwmon aggiornati", NULL,
                        "ghostty -e sh -lc 'watch -n 1 sensors'", TRUE);
    menu_add_shell_item(
        app, "drive-harddisk-symbolic", "Stato NVMe",
        "SMART e indicatori di salute del disco", NULL,
        "ghostty -e sh -lc 'sudo smartctl -a /dev/nvme0; printf \\\"\\nPremi Invio…\\\"; read -r _'",
        TRUE);

    menu_add_section(app, "PROFILO TERMICO");
    runtime->profile_rows[0] = anto_hardware_add_profile(
        runtime, "battery-low-symbolic", "Silenzioso",
        "Riduce consumi e calore", "power-saver",
        &runtime->profile_badges[0]);
    runtime->profile_rows[1] = anto_hardware_add_profile(
        runtime, "battery-good-symbolic", "Bilanciato",
        "Gestione automatica quotidiana", "balanced",
        &runtime->profile_badges[1]);
    runtime->profile_rows[2] = anto_hardware_add_profile(
        runtime, "battery-full-symbolic", "Prestazioni",
        "Favorisce potenza e reattività", "performance",
        &runtime->profile_badges[2]);
    menu_set_footer(
        app, "Metriche live aggiornate in-place · nessun comando blocca GTK");
    g_weak_ref_set(&runtime->root, G_OBJECT(root));
    runtime->full_view = TRUE;
    anto_hardware_apply_snapshot(runtime, snapshot);
}

void menu_show_hardware(MenuApp *app) {
    HardwareRuntime *runtime = anto_hardware_runtime_get(app);
    if (!anto_hardware_root_is_current(runtime))
        anto_hardware_render(runtime, runtime->snapshot);
    anto_hardware_start_snapshot(app);
}
