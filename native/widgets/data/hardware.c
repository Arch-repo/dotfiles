#include "widgets.h"
#include <stdio.h>
#include <sys/statvfs.h>
static void replace(char **value, char *next) {
  g_free(*value);
  *value = next;
}
void widget_hardware_update(WidgetStore *s) {
  g_autofree char *content = NULL;
  if (g_file_get_contents("/proc/stat", &content, NULL, NULL)) {
    guint64 u = 0, n = 0, y = 0, idle = 0, wait = 0, irq = 0, soft = 0,
            steal = 0;
    if (sscanf(content,
               "cpu %" G_GUINT64_FORMAT " %" G_GUINT64_FORMAT
               " %" G_GUINT64_FORMAT " %" G_GUINT64_FORMAT " %" G_GUINT64_FORMAT
               " %" G_GUINT64_FORMAT " %" G_GUINT64_FORMAT
               " %" G_GUINT64_FORMAT,
               &u, &n, &y, &idle, &wait, &irq, &soft, &steal) >= 4) {
      guint64 total = u + n + y + idle + wait + irq + soft + steal,
              quiet = idle + wait;
      if (s->cpu_total && total > s->cpu_total && quiet >= s->cpu_idle)
        s->cpu = CLAMP(
            1.0 - (double)(quiet - s->cpu_idle) / (total - s->cpu_total), 0, 1);
      s->cpu_total = total;
      s->cpu_idle = quiet;
    }
  }
  g_clear_pointer(&content, g_free);
  if (g_file_get_contents("/proc/meminfo", &content, NULL, NULL)) {
    guint64 total = 0, available = 0;
    g_auto(GStrv) lines = g_strsplit(content, "\n", -1);
    for (guint i = 0; lines[i]; i++) {
      sscanf(lines[i], "MemTotal: %" G_GUINT64_FORMAT, &total);
      sscanf(lines[i], "MemAvailable: %" G_GUINT64_FORMAT, &available);
    }
    if (total && total >= available) {
      s->memory = (double)(total - available) / total;
      replace(&s->memory_text, g_strdup_printf("%.1f / %.1f GiB",
                                               (total - available) / 1048576.0,
                                               total / 1048576.0));
    }
  }
  struct statvfs fs;
  if (!statvfs(g_get_home_dir(), &fs) && fs.f_blocks)
    s->disk = 1.0 - (double)fs.f_bavail / fs.f_blocks;
  g_autoptr(GDir) supplies = g_dir_open("/sys/class/power_supply", 0, NULL);
  const char *name;
  gboolean battery = FALSE;
  while (supplies && (name = g_dir_read_name(supplies))) {
    g_autofree char *type_path = g_build_filename("/sys/class/power_supply",
                                                  name, "type", NULL),
                    *type = NULL;
    if (!g_file_get_contents(type_path, &type, NULL, NULL) ||
        !g_str_has_prefix(type, "Battery"))
      continue;
    g_autofree char *path = g_build_filename("/sys/class/power_supply", name,
                                             "capacity", NULL),
                    *capacity = NULL;
    if (!g_file_get_contents(path, &capacity, NULL, NULL))
      continue;
    s->battery = CLAMP(g_ascii_strtod(capacity, NULL) / 100.0, 0, 1);
    g_autofree char *status_path = g_build_filename("/sys/class/power_supply",
                                                    name, "status", NULL),
                    *status = NULL;
    g_file_get_contents(status_path, &status, NULL, NULL);
    replace(&s->battery_text,
            g_strdup_printf(
                "%.0f%% · %s", s->battery * 100,
                status && g_str_has_prefix(status, "Charging") ? "In carica"
                : status && g_str_has_prefix(status, "Full") ? "Carica completa"
                                                             : "Batteria"));
    battery = TRUE;
    break;
  }
  if (!battery) {
    s->battery = -1;
    replace(&s->battery_text, g_strdup("Alimentazione di rete"));
  }
  widget_store_changed(s, W_CHANGED_HARDWARE);
}
