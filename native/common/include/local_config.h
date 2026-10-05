#pragma once

#include <gio/gio.h>

typedef enum {
    ANTO_LOCAL_CONFIG_ERROR_INVALID_PATH,
    ANTO_LOCAL_CONFIG_ERROR_IO,
    ANTO_LOCAL_CONFIG_ERROR_EXISTS,
} AntoLocalConfigError;

#define ANTO_LOCAL_CONFIG_ERROR (anto_local_config_error_quark())

GQuark anto_local_config_error_quark(void);
gboolean anto_local_config_validate_relative(const char *relative,
                                             GError **error);
char *anto_local_config_root(void);
char *anto_local_config_path(const char *relative, GError **error);
gboolean anto_local_config_ensure_root(GError **error);
gboolean anto_local_config_ensure_parent(const char *relative,
                                         GError **error);
int anto_local_config_lock(const char *relative, gboolean wait,
                           GError **error);
void anto_local_config_unlock(int descriptor);
gboolean anto_local_config_write(const char *relative,
                                 const void *data, gsize length,
                                 int mode, GError **error);
gboolean anto_local_config_write_text(const char *relative,
                                      const char *text, int mode,
                                      GError **error);
char *anto_local_config_read_text(const char *relative,
                                  gsize *length, GError **error);
gboolean anto_local_config_migrate_file(const char *source,
                                        const char *relative,
                                        gboolean overwrite,
                                        GError **error);
