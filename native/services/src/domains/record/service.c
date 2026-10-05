#include "service.h"
/* Application boundary: validate before accessing a provider or starting a device command. */
int anto_record_execute(int argc, char **argv);
static const BackendOperation operations[] = {
    {"area", 0, 0, TRUE},
    {"window", 0, 0, TRUE},
    {"monitor", 0, 0, TRUE},
    {"monitor-audio", 0, 0, TRUE},
    {"stop", 0, 0, TRUE},
    {"status", 0, 0, FALSE},
    {"waybar-json", 0, 0, FALSE},
    {"open", 0, 0, TRUE},
};
const BackendService anto_service_record = {"record", operations, G_N_ELEMENTS(operations), anto_record_execute};
int backend_record_main(int argc, char **argv) {
    return backend_service_call(&anto_service_record, argc, argv);
}
