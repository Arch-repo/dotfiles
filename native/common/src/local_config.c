#include "local_config.h"

#include <errno.h>
#include <fcntl.h>
#include <glib/gstdio.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

GQuark anto_local_config_error_quark(void) {
    return g_quark_from_static_string("anto-local-config-error");
}

static gboolean path_error(GError **error, AntoLocalConfigError code,
                           const char *message, const char *value) {
    g_set_error(error, ANTO_LOCAL_CONFIG_ERROR, code, "%s%s",
                message ? message : "", value ? value : "");
    return FALSE;
}

gboolean anto_local_config_validate_relative(const char *relative,
                                             GError **error) {
    if (!relative || !*relative)
        return path_error(error, ANTO_LOCAL_CONFIG_ERROR_INVALID_PATH,
                          "Percorso locale vuoto: ", "");
    if (g_path_is_absolute(relative) || strlen(relative) > 1024)
        return path_error(error, ANTO_LOCAL_CONFIG_ERROR_INVALID_PATH,
                          "Percorso locale non valido: ", relative);
    g_auto(GStrv) components = g_strsplit(relative, "/", -1);
    for (guint index = 0; components[index]; index++) {
        const char *component = components[index];
        if (!*component || g_strcmp0(component, ".") == 0 ||
            g_strcmp0(component, "..") == 0 ||
            strlen(component) > 255)
            return path_error(error,
                              ANTO_LOCAL_CONFIG_ERROR_INVALID_PATH,
                              "Componente locale non valido: ",
                              component);
        for (const char *cursor = component; *cursor; cursor++)
            if ((guchar)*cursor < 0x20 || *cursor == 0x7f)
                return path_error(
                    error, ANTO_LOCAL_CONFIG_ERROR_INVALID_PATH,
                    "Carattere non valido nel percorso: ", component);
    }
    return TRUE;
}

char *anto_local_config_root(void) {
    const char *override = g_getenv("ANTO_LOCAL_CONFIG_ROOT");
    if (override && *override)
        return g_canonicalize_filename(override, NULL);
    const char *config = g_get_user_config_dir();
    if (!config || !*config)
        return g_build_filename(g_get_home_dir(), ".config",
                                "anto426-local", NULL);
    return g_build_filename(config, "anto426-local", NULL);
}

char *anto_local_config_path(const char *relative, GError **error) {
    if (!anto_local_config_validate_relative(relative, error))
        return NULL;
    g_autofree char *root = anto_local_config_root();
    return g_build_filename(root, relative, NULL);
}

static gboolean ensure_directory(const char *directory, GError **error) {
    if (g_mkdir_with_parents(directory, 0700) == 0) return TRUE;
    return path_error(error, ANTO_LOCAL_CONFIG_ERROR_IO,
                      "Directory locale non creata: ", directory);
}

gboolean anto_local_config_ensure_root(GError **error) {
    g_autofree char *root = anto_local_config_root();
    return ensure_directory(root, error);
}

gboolean anto_local_config_ensure_parent(const char *relative,
                                         GError **error) {
    g_autofree char *path = anto_local_config_path(relative, error);
    if (!path) return FALSE;
    g_autofree char *parent = g_path_get_dirname(path);
    return ensure_directory(parent, error);
}

int anto_local_config_lock(const char *relative, gboolean wait,
                           GError **error) {
    g_autofree char *lock_relative =
        g_strdup_printf(".locks/%s.lock", relative ? relative : "");
    if (!anto_local_config_ensure_parent(lock_relative, error)) return -1;
    g_autofree char *path =
        anto_local_config_path(lock_relative, error);
    if (!path) return -1;
    int descriptor = open(path, O_CREAT | O_RDWR, 0600);
    if (descriptor < 0) {
        g_set_error(error, ANTO_LOCAL_CONFIG_ERROR,
                    ANTO_LOCAL_CONFIG_ERROR_IO,
                    "Lock locale non aperto (%s): %s", path,
                    g_strerror(errno));
        return -1;
    }
    fcntl(descriptor, F_SETFD, FD_CLOEXEC);
    int flags = LOCK_EX | (wait ? 0 : LOCK_NB);
    if (flock(descriptor, flags) != 0) {
        g_set_error(error, ANTO_LOCAL_CONFIG_ERROR,
                    ANTO_LOCAL_CONFIG_ERROR_IO,
                    "Lock locale non acquisito (%s): %s", path,
                    g_strerror(errno));
        close(descriptor);
        return -1;
    }
    return descriptor;
}

void anto_local_config_unlock(int descriptor) {
    if (descriptor < 0) return;
    flock(descriptor, LOCK_UN);
    close(descriptor);
}

static gboolean write_all(int descriptor, const guint8 *data, gsize length,
                          GError **error) {
    gsize written = 0;
    while (written < length) {
        ssize_t result =
            write(descriptor, data + written, length - written);
        if (result > 0) {
            written += (gsize)result;
            continue;
        }
        if (result < 0 && errno == EINTR) continue;
        g_set_error(error, ANTO_LOCAL_CONFIG_ERROR,
                    ANTO_LOCAL_CONFIG_ERROR_IO,
                    "Scrittura locale incompleta: %s",
                    g_strerror(errno));
        return FALSE;
    }
    return TRUE;
}

static gboolean fsync_directory(const char *directory, GError **error) {
    int flags = O_RDONLY;
#ifdef O_DIRECTORY
    flags |= O_DIRECTORY;
#endif
    int descriptor = open(directory, flags);
    if (descriptor < 0) {
        g_set_error(error, ANTO_LOCAL_CONFIG_ERROR,
                    ANTO_LOCAL_CONFIG_ERROR_IO,
                    "Directory locale non apribile (%s): %s",
                    directory, g_strerror(errno));
        return FALSE;
    }
    gboolean ok = fsync(descriptor) == 0;
    if (!ok)
        g_set_error(error, ANTO_LOCAL_CONFIG_ERROR,
                    ANTO_LOCAL_CONFIG_ERROR_IO,
                    "Sync directory locale fallita (%s): %s",
                    directory, g_strerror(errno));
    close(descriptor);
    return ok;
}

gboolean anto_local_config_write(const char *relative,
                                 const void *data, gsize length,
                                 int mode, GError **error) {
    if ((!data && length) ||
        !anto_local_config_ensure_parent(relative, error))
        return FALSE;
    g_autofree char *path = anto_local_config_path(relative, error);
    if (!path) return FALSE;
    g_autofree char *directory = g_path_get_dirname(path);
    g_autofree char *base = g_path_get_basename(path);
    g_autofree char *template =
        g_strdup_printf("%s/.%s.tmp.XXXXXX", directory, base);
    int descriptor = g_mkstemp_full(template, O_RDWR, mode > 0 ? mode : 0600);
    if (descriptor < 0) {
        g_set_error(error, ANTO_LOCAL_CONFIG_ERROR,
                    ANTO_LOCAL_CONFIG_ERROR_IO,
                    "File temporaneo locale non creato: %s",
                    g_strerror(errno));
        return FALSE;
    }
    gboolean ok = write_all(descriptor, data, length, error);
    if (ok && fchmod(descriptor, mode > 0 ? mode : 0600) != 0) {
        g_set_error(error, ANTO_LOCAL_CONFIG_ERROR,
                    ANTO_LOCAL_CONFIG_ERROR_IO,
                    "Permessi locali non applicati: %s",
                    g_strerror(errno));
        ok = FALSE;
    }
    if (ok && fsync(descriptor) != 0) {
        g_set_error(error, ANTO_LOCAL_CONFIG_ERROR,
                    ANTO_LOCAL_CONFIG_ERROR_IO,
                    "Sync file locale fallita: %s", g_strerror(errno));
        ok = FALSE;
    }
    if (close(descriptor) != 0 && ok) {
        g_set_error(error, ANTO_LOCAL_CONFIG_ERROR,
                    ANTO_LOCAL_CONFIG_ERROR_IO,
                    "Chiusura file locale fallita: %s", g_strerror(errno));
        ok = FALSE;
    }
    if (ok && g_rename(template, path) != 0) {
        g_set_error(error, ANTO_LOCAL_CONFIG_ERROR,
                    ANTO_LOCAL_CONFIG_ERROR_IO,
                    "Sostituzione atomica locale fallita: %s",
                    g_strerror(errno));
        ok = FALSE;
    }
    if (ok) ok = fsync_directory(directory, error);
    if (!ok) g_unlink(template);
    return ok;
}

gboolean anto_local_config_write_text(const char *relative,
                                      const char *text, int mode,
                                      GError **error) {
    const char *value = text ? text : "";
    return anto_local_config_write(relative, value, strlen(value),
                                   mode, error);
}

char *anto_local_config_read_text(const char *relative,
                                  gsize *length, GError **error) {
    g_autofree char *path = anto_local_config_path(relative, error);
    if (!path) return NULL;
    char *content = NULL;
    if (!g_file_get_contents(path, &content, length, error))
        return NULL;
    return content;
}

gboolean anto_local_config_migrate_file(const char *source,
                                        const char *relative,
                                        gboolean overwrite,
                                        GError **error) {
    if (!source || !g_path_is_absolute(source))
        return path_error(error,
                          ANTO_LOCAL_CONFIG_ERROR_INVALID_PATH,
                          "Sorgente migrazione non assoluta: ",
                          source);
    g_autofree char *destination =
        anto_local_config_path(relative, error);
    if (!destination) return FALSE;
    if (!overwrite && g_file_test(destination, G_FILE_TEST_EXISTS))
        return path_error(error, ANTO_LOCAL_CONFIG_ERROR_EXISTS,
                          "Destinazione locale già presente: ",
                          relative);
    g_autofree char *content = NULL;
    gsize length = 0;
    if (!g_file_get_contents(source, &content, &length, error))
        return FALSE;
    struct stat info = {0};
    int mode = stat(source, &info) == 0 ? (info.st_mode & 0777) : 0600;
    return anto_local_config_write(relative, content, length, mode, error);
}
