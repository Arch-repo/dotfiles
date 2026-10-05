#include "service.h"
/* Application boundary: validate before accessing a provider or starting a device command. */
int anto_network_execute(int argc, char **argv);
static const BackendOperation operations[] = {
    {"snapshot", 0, 0, FALSE},
    {"status", 0, 0, FALSE},
    {"toggle", 0, 0, TRUE},
    {"radio", 1, 1, TRUE},
    {"rescan", 0, 0, TRUE},
    {"connect-saved", 1, 1, TRUE},
    {"connect-open", 1, 1, TRUE},
    {"connect-secure", 1, 1, TRUE},
    {"disconnect", 1, 1, TRUE},
};
const BackendService anto_service_network = {"network", operations, G_N_ELEMENTS(operations), anto_network_execute};
int backend_network_main(int argc, char **argv) {
    return backend_service_call(&anto_service_network, argc, argv);
}
