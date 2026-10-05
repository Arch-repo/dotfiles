#include "service.h"
/* Application boundary: validate before accessing a provider or starting a device command. */
int anto_system_execute(int argc, char **argv);
static const BackendOperation operations[] = {
    {"snapshot", 0, 0, FALSE},
    {"hardware-snapshot", 0, 0, FALSE},
};
const BackendService anto_service_system = {"system", operations, G_N_ELEMENTS(operations), anto_system_execute};
int backend_system_main(int argc, char **argv) {
    return backend_service_call(&anto_service_system, argc, argv);
}
