#define _DEFAULT_SOURCE

#include "backend.h"
#include "service.h"
#include "local_config.h"

#include <errno.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

enum {
    CONFIG_SCAN_MAX_DEPTH = 64,
    CONFIG_SCAN_MAX_ENTRIES = 100000,
};

typedef struct {
    guint64 files;
    guint64 directories;
    guint64 links;
    guint64 other;
    guint64 bytes;
    guint64 entries;
    gboolean truncated;
} ConfigStats;

static gint name_compare(gconstpointer left, gconstpointer right) {
    const char *const *left_name = left;
    const char *const *right_name = right;
    return g_strcmp0(*left_name, *right_name);
}

static void print_path_field(const char *kind, const char *relative,
                             guint64 size, gboolean show_size) {
    fputs(kind, stdout);
    fputc('\t', stdout);
    backend_print_field(relative);
    if (show_size)
        g_print("\t%" G_GUINT64_FORMAT, size);
    fputc('\n', stdout);
}

static gboolean scan_directory(const char *root, const char *relative,
                               guint depth, gboolean emit,
                               ConfigStats *stats, GError **error) {
    g_autofree char *directory =
        relative && *relative
            ? g_build_filename(root, relative, NULL)
            : g_strdup(root);
    g_autoptr(GDir) handle = g_dir_open(directory, 0, error);
    if (!handle) return FALSE;

    g_autoptr(GPtrArray) names =
        g_ptr_array_new_with_free_func(g_free);
    const char *name = NULL;
    while ((name = g_dir_read_name(handle)))
        g_ptr_array_add(names, g_strdup(name));
    g_ptr_array_sort(names, name_compare);

    for (guint index = 0; index < names->len; index++) {
        if (stats->entries >= CONFIG_SCAN_MAX_ENTRIES) {
            stats->truncated = TRUE;
            break;
        }
        const char *child_name = g_ptr_array_index(names, index);
        g_autofree char *child_relative =
            relative && *relative
                ? g_build_filename(relative, child_name, NULL)
                : g_strdup(child_name);
        g_autofree char *child =
            g_build_filename(root, child_relative, NULL);
        GStatBuf info = {0};
        if (g_lstat(child, &info) != 0) {
            if (errno == ENOENT) continue;
            g_set_error(error, G_FILE_ERROR,
                        g_file_error_from_errno(errno),
                        "Impossibile ispezionare %s: %s",
                        child, g_strerror(errno));
            return FALSE;
        }
        stats->entries++;
        if (S_ISLNK(info.st_mode)) {
            stats->links++;
            if (emit) print_path_field("LINK", child_relative, 0, FALSE);
            continue;
        }
        if (S_ISDIR(info.st_mode)) {
            stats->directories++;
            if (emit) print_path_field("DIR", child_relative, 0, FALSE);
            if (depth >= CONFIG_SCAN_MAX_DEPTH) {
                stats->truncated = TRUE;
                continue;
            }
            if (!scan_directory(root, child_relative, depth + 1,
                                emit, stats, error))
                return FALSE;
            if (stats->truncated &&
                stats->entries >= CONFIG_SCAN_MAX_ENTRIES)
                break;
            continue;
        }
        if (S_ISREG(info.st_mode)) {
            guint64 size = info.st_size > 0 ? (guint64)info.st_size : 0;
            stats->files++;
            stats->bytes += size;
            if (emit)
                print_path_field("FILE", child_relative, size, TRUE);
            continue;
        }
        stats->other++;
        if (emit) print_path_field("OTHER", child_relative, 0, FALSE);
    }
    return TRUE;
}

static gboolean inspect_root(const char *root, gboolean emit,
                             ConfigStats *stats, gboolean *exists,
                             GError **error) {
    GStatBuf info = {0};
    if (g_stat(root, &info) != 0) {
        if (errno == ENOENT) {
            *exists = FALSE;
            return TRUE;
        }
        g_set_error(error, G_FILE_ERROR,
                    g_file_error_from_errno(errno),
                    "Impossibile ispezionare la radice %s: %s",
                    root, g_strerror(errno));
        return FALSE;
    }
    *exists = TRUE;
    if (!S_ISDIR(info.st_mode)) {
        g_set_error(error, G_FILE_ERROR, G_FILE_ERROR_NOTDIR,
                    "La radice locale non è una directory: %s", root);
        return FALSE;
    }
    return scan_directory(root, "", 0, emit, stats, error);
}

static int config_status(void) {
    g_autofree char *root = anto_local_config_root();
    ConfigStats stats = {0};
    gboolean exists = FALSE;
    g_autoptr(GError) error = NULL;
    if (!inspect_root(root, FALSE, &stats, &exists, &error))
        return backend_error(
            1, "config-status",
            error ? error->message :
                    "Configurazione locale non ispezionabile");
    fputs("root\t", stdout);
    backend_print_field(root);
    fputc('\n', stdout);
    g_print("exists\t%s\n", exists ? "true" : "false");
    g_print("readable\t%s\n",
            exists && access(root, R_OK | X_OK) == 0
                ? "true" : "false");
    g_print("writable\t%s\n",
            exists && access(root, W_OK | X_OK) == 0
                ? "true" : "false");
    g_print("files\t%" G_GUINT64_FORMAT "\n", stats.files);
    g_print("directories\t%" G_GUINT64_FORMAT "\n",
            stats.directories);
    g_print("links\t%" G_GUINT64_FORMAT "\n", stats.links);
    g_print("other\t%" G_GUINT64_FORMAT "\n", stats.other);
    g_print("bytes\t%" G_GUINT64_FORMAT "\n", stats.bytes);
    g_print("truncated\t%s\n",
            stats.truncated ? "true" : "false");
    return 0;
}

static int config_tree(void) {
    g_autofree char *root = anto_local_config_root();
    ConfigStats stats = {0};
    gboolean exists = FALSE;
    g_autoptr(GError) error = NULL;
    fputs("ROOT\t", stdout);
    backend_print_field(root);
    fputc('\n', stdout);
    if (!inspect_root(root, TRUE, &stats, &exists, &error))
        return backend_error(
            1, "config-tree",
            error ? error->message :
                    "Configurazione locale non ispezionabile");
    if (!exists) {
        puts("MISSING");
        return 0;
    }
    if (!stats.entries) puts("EMPTY");
    if (stats.truncated) puts("TRUNCATED");
    return 0;
}

static int config_usage(void) {
    g_printerr(
        "Uso: anto-config "
        "{root|status|tree|get REL|"
        "migrate SOURCE_ABSOLUTE REL [--overwrite]}\n"
        "     anto-menu-backend config "
        "{root|status|tree|get REL|"
        "migrate SOURCE_ABSOLUTE REL [--overwrite]}\n");
    return 2;
}

int anto_config_execute(int argc, char **argv) {
    extern const BackendService anto_service_config;
    int operation = backend_operation_index(&anto_service_config, argv[0]);
    if (operation == 1) return config_status();
    if (operation == 2) return config_tree();
    if (operation == 0) { g_autofree char *root = anto_local_config_root(); puts(root); return 0; }
    g_autoptr(GError) error = NULL;
    if (operation == 3) {
        g_autofree char *path = anto_local_config_path(argv[1], &error);
        if (!path) return backend_error(2, "invalid-path", error->message);
        puts(path); return 0;
    }
    gboolean overwrite = argc == 4 && g_strcmp0(argv[3], "--overwrite") == 0;
    if (argc == 4 && !overwrite) return config_usage();
    if (!anto_local_config_migrate_file(argv[1], argv[2], overwrite, &error))
        return backend_error(error && error->domain == ANTO_LOCAL_CONFIG_ERROR && error->code == ANTO_LOCAL_CONFIG_ERROR_EXISTS ? 3 : 1,
                              "migrate", error ? error->message : "Migrazione locale fallita");
    g_autofree char *path = anto_local_config_path(argv[2], NULL);
    puts(path); return 0;
}
