#include "menu.h"

typedef struct {
    char *domain;
    char *operation;
    char *argument;
    char *value;
    gboolean close_after;
} BackendItem;

char *menu_backend_path(void) {
    const char *override = g_getenv("ANTO_MENU_BACKEND");
    if (override && *override) return g_strdup(override);
    return menu_home_path(
        ".local/libexec/anto-menu/anto-menu-backend");
}

void menu_spawn_backend(MenuApp *app, const char *domain,
                        const char *operation, const char *argument,
                        const char *value, gboolean close_after) {
    g_autofree char *backend = menu_backend_path();
    const char *argv[] = {
        backend, domain, operation, argument, value, NULL,
    };
    menu_spawn(app, argv, close_after);
}

static void backend_item_free(gpointer data) {
    BackendItem *item = data;
    if (!item) return;
    g_free(item->domain);
    g_free(item->operation);
    g_free(item->argument);
    g_free(item->value);
    g_free(item);
}

static BackendItem *backend_item_new(const char *domain,
                                     const char *operation,
                                     const char *argument,
                                     const char *value,
                                     gboolean close_after) {
    BackendItem *item = g_new0(BackendItem, 1);
    item->domain = g_strdup(domain);
    item->operation = g_strdup(operation);
    item->argument = g_strdup(argument);
    item->value = g_strdup(value);
    item->close_after = close_after;
    return item;
}

static void backend_item_activate(MenuApp *app, gpointer data) {
    BackendItem *item = data;
    menu_spawn_backend(app, item->domain, item->operation,
                       item->argument, item->value,
                       item->close_after);
}

void menu_add_backend_item(MenuApp *app, const char *icon,
                           const char *title, const char *subtitle,
                           const char *badge, const char *domain,
                           const char *operation, const char *argument,
                           const char *value, gboolean close_after) {
    BackendItem *item = backend_item_new(
        domain, operation, argument, value, close_after);
    menu_add_item(app, icon, title, subtitle, badge,
                  backend_item_activate, item, backend_item_free);
}

void menu_add_backend_tile(MenuApp *app, const char *icon,
                           const char *title, const char *subtitle,
                           const char *badge, const char *domain,
                           const char *operation, const char *argument,
                           const char *value, gboolean close_after) {
    BackendItem *item = backend_item_new(
        domain, operation, argument, value, close_after);
    menu_add_tile(app, icon, title, subtitle, badge,
                  backend_item_activate, item, backend_item_free);
}
