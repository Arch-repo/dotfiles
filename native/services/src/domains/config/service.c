#include "service.h"
/* Application boundary: validate before accessing a provider or starting a device command. */
int anto_config_execute(int argc, char **argv);
static const BackendOperation operations[] = {
    {"root", 0, 0, FALSE},
    {"status", 0, 0, FALSE},
    {"tree", 0, 0, FALSE},
    {"get", 1, 1, FALSE},
    {"migrate", 2, 3, TRUE},
};
const BackendService anto_service_config = {"config", operations, G_N_ELEMENTS(operations), anto_config_execute};
int backend_config_main(int argc, char **argv) {
    return backend_service_call(&anto_service_config, argc, argv);
}
