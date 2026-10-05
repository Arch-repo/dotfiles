#include "backend.h"
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

gboolean backend_write_atomic(const char *path, const char *text, gint mode, GError **error) {
    g_autofree char *directory = g_path_get_dirname(path);
    g_autofree char *temporary = NULL;
    if (g_mkdir_with_parents(directory, 0700) != 0) goto failed;
    temporary = g_strconcat(path, ".tmp.XXXXXX", NULL);
    int fd = g_mkstemp_full(temporary, O_WRONLY | O_CLOEXEC, mode);
    if (fd < 0) goto failed;
    const char *cursor = text ? text : "";
    gsize remaining = strlen(cursor);
    gboolean ok = TRUE;
    int saved = 0;
    while (remaining) {
        ssize_t length = write(fd, cursor, remaining);
        if (length < 0 && errno == EINTR) continue;
        if (length <= 0) { ok = FALSE; saved = errno ? errno : EIO; break; }
        cursor += length; remaining -= length;
    }
    if (ok && fsync(fd) != 0) { ok = FALSE; saved = errno; }
    if (close(fd) != 0 && ok) { ok = FALSE; saved = errno; }
    if (ok && g_rename(temporary, path) != 0) { ok = FALSE; saved = errno; }
    if (!ok) { g_unlink(temporary); errno = saved; goto failed; }
    /* Commit the rename as well as its contents on filesystems supporting directory fsync. */
    int parent = open(directory, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (parent >= 0) { (void)fsync(parent); close(parent); }
    return TRUE;
failed:
    g_set_error(error, G_FILE_ERROR, g_file_error_from_errno(errno), "Salvataggio atomico fallito: %s", g_strerror(errno));
    return FALSE;
}
