#include "service.h"
/* Application boundary: validate before accessing a provider or starting a device command. */
int anto_session_execute(int argc, char **argv);
static const BackendOperation operations[] = {
    {"lock", 0, 0, TRUE},
    {"suspend", 0, 0, TRUE},
    {"logout", 0, 0, TRUE},
    {"reboot", 0, 0, TRUE},
    {"poweroff", 0, 0, TRUE},
};
const BackendService anto_service_session = {"session", operations, G_N_ELEMENTS(operations), anto_session_execute};
int backend_session_main(int argc, char **argv) {
    return backend_service_call(&anto_service_session, argc, argv);
}
