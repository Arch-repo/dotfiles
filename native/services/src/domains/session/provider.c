#include "backend.h"
#include "service.h"
int anto_session_execute(int argc, char **argv) {
    (void)argc;
    extern const BackendService anto_service_session;
    int operation = backend_operation_index(&anto_service_session, argv[0]);
    if (operation < 0) return backend_error(2, "invalid-session-action", "Azione non valida");
    if (backend_dry_run()) { g_print("DRYRUN\tsession\t%s\n", argv[0]); return 0; }
    const char *program = operation == 0 ? backend_program("ANTO_MENU_HYPRLOCK", "hyprlock")
                        : operation == 2 ? backend_program("ANTO_MENU_HYPRCTL", "hyprctl")
                        : backend_program("ANTO_MENU_SYSTEMCTL", "systemctl");
    const char *command[] = {program, operation == 0 ? NULL : operation == 2 ? "dispatch" : argv[0],
                             operation == 2 ? "exit" : NULL, NULL};
    return backend_command_forward(command, NULL);
}
