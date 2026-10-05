#include "service.h"
/* Application boundary: validate before accessing a provider or starting a device command. */
int anto_energy_execute(int argc, char **argv);
static const BackendOperation operations[] = {
    {"snapshot", 0, 0, FALSE},
    {"status", 0, 0, FALSE},
    {"list", 0, 0, FALSE},
    {"set", 1, 1, TRUE},
    {"brightness-set", 1, 1, TRUE},
};
const BackendService anto_service_energy = {"energy", operations, G_N_ELEMENTS(operations), anto_energy_execute};
int backend_energy_main(int argc, char **argv) {
    return backend_service_call(&anto_service_energy, argc, argv);
}
