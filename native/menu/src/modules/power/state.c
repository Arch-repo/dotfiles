#include "internal.h"

char *anto_power_session_uptime(void) {
    g_autofree char *text = NULL;
    if (!g_file_get_contents("/proc/uptime", &text, NULL, NULL))
        return g_strdup("stato pronto");
    char *end = NULL;
    double seconds = g_ascii_strtod(text, &end);
    if (!end || end == text || seconds < 0)
        return g_strdup("stato pronto");
    guint64 minutes = (guint64)(seconds / 60.0);
    guint64 days = minutes / (24 * 60);
    guint64 hours = (minutes / 60) % 24;
    minutes %= 60;
    if (days)
        return g_strdup_printf("%" G_GUINT64_FORMAT
                               "g %" G_GUINT64_FORMAT "h",
                               days, hours);
    if (hours)
        return g_strdup_printf("%" G_GUINT64_FORMAT
                               "h %" G_GUINT64_FORMAT "min",
                               hours, minutes);
    return g_strdup_printf("%" G_GUINT64_FORMAT "min", minutes);
}
