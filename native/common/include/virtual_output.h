#pragma once

#include <glib.h>

typedef struct {
    char *name;
    int width;
    int height;
    double refresh;
    double scale;
    int x;
    int y;
    int transform;
    gboolean persistent;
} MenuVirtualOutput;

void menu_virtual_output_free(gpointer data);
GPtrArray *menu_virtual_output_load(GError **error);
gboolean menu_virtual_output_is_managed(const char *name);

/*
 * Handles the `virtual-output` direct-action namespace.
 * Returns -1 when argv does not address this backend.
 */
int menu_virtual_output_action(int argc, char **argv);
