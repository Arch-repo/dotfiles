#include "apply.hpp"
#include <cerrno>
#include <fcntl.h>
#include <glib/gstdio.h>
#include <signal.h>
#include <unistd.h>

namespace anto {
static void detach(gpointer) {
  setsid();
  signal(SIGHUP, SIG_IGN);
}

bool launch_wallpaper_apply(const std::string &path, const std::string &target,
                            GError **error) {
  g_autofree char *executable = g_file_read_link("/proc/self/exe", error);
  if (!executable)
    return false;
  g_autofree char *binary_directory = g_path_get_dirname(executable);
  const char *worker_override = g_getenv("ANTO426_WALLPAPER_WORKER");
  g_autofree char *worker = worker_override && *worker_override
      ? g_strdup(worker_override)
      : g_build_filename(binary_directory, "anto-wallpaper", nullptr);
  g_autofree char *directory =
      g_build_filename(g_get_user_state_dir(), "anto426", nullptr);
  if (g_mkdir_with_parents(directory, 0700) != 0) {
    g_set_error(error, G_FILE_ERROR, g_file_error_from_errno(errno),
                "Impossibile creare la cartella dei log: %s",
                g_strerror(errno));
    return false;
  }
  g_autofree char *log =
      g_build_filename(directory, "wallpaper-jobs.log", nullptr);
  int output =
      g_open(log, O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC | O_NOFOLLOW, 0600);
  if (output < 0) {
    g_set_error(error, G_FILE_ERROR, g_file_error_from_errno(errno),
                "Impossibile aprire il log degli sfondi: %s",
                g_strerror(errno));
    return false;
  }
  int errors = dup(output);
  if (errors < 0) {
    int failure = errno;
    close(output);
    g_set_error(error, G_FILE_ERROR, g_file_error_from_errno(failure),
                "Impossibile aprire il log degli errori: %s",
                g_strerror(failure));
    return false;
  }
  g_autoptr(GSubprocessLauncher) launcher =
      g_subprocess_launcher_new(G_SUBPROCESS_FLAGS_NONE);
  g_subprocess_launcher_set_child_setup(launcher, detach, nullptr, nullptr);
  g_subprocess_launcher_set_stdin_file_path(launcher, "/dev/null");
  g_subprocess_launcher_take_stdout_fd(launcher, output);
  g_subprocess_launcher_take_stderr_fd(launcher, errors);
  const char *arguments[] = {worker, "--apply-worker", path.c_str(),
                             target.c_str(), nullptr};
  g_autoptr(GSubprocess) process =
      g_subprocess_launcher_spawnv(launcher, arguments, error);
  return process != nullptr;
}

int run_wallpaper_apply(const std::string &path, const std::string &target) {
  const bool boot = target == "__boot_login__";
  const char *override = g_getenv("ANTO426_WALLPAPER_CORE");
  const char *boot_override = g_getenv("ANTO426_WALLPAPER_BOOT_APPLY_SCRIPT");
  std::string program = boot ? boot_override && *boot_override ? boot_override
                                 : std::string(g_get_user_config_dir()) + "/anto426/wallpaper_boot_apply.sh"
                        : override && *override
                            ? override
                            : std::string(g_get_home_dir()) +
                                  "/.local/libexec/anto426/anto-wallpaper-core";
  const char *desktop[] = {program.c_str(), "apply", path.c_str(), nullptr};
  const char *system[] = {program.c_str(), path.c_str(), nullptr};
  g_autoptr(GSubprocessLauncher) launcher =
      g_subprocess_launcher_new(G_SUBPROCESS_FLAGS_NONE);
  g_subprocess_launcher_setenv(launcher, "ANTO426_WALLPAPER_OUTPUT",
                               target.c_str(), TRUE);
  g_autoptr(GError) error = nullptr;
  g_autoptr(GSubprocess) process =
      g_subprocess_launcher_spawnv(launcher, boot ? system : desktop, &error);
  if (process && g_subprocess_wait_check(process, nullptr, &error))
    return 0;
  const char *message =
      error ? error->message : "Impossibile applicare lo sfondo";
  g_printerr("Sfondo %s (%s): %s\n", path.c_str(), target.c_str(), message);
  const char *notification[] = {"notify-send",
                                "--app-name=Anto Desktop",
                                "--urgency=critical",
                                "Sfondo non applicato",
                                message,
                                nullptr};
  g_autoptr(GSubprocess) notify = g_subprocess_newv(
      notification, G_SUBPROCESS_FLAGS_STDOUT_SILENCE, nullptr);
  return 1;
}
} // namespace anto
