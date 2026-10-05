#include "service.h"
/* Application boundary: validate before accessing a provider or starting a device command. */
int anto_bluetooth_execute(int argc, char **argv);
static const BackendOperation operations[] = {
    {"snapshot", 0, 0, FALSE},
    {"status", 0, 0, FALSE},
    {"controllers", 0, 0, FALSE},
    {"devices", 0, 1, FALSE},
    {"info", 1, 1, FALSE},
    {"battery", 1, 1, FALSE},
    {"audio-profiles", 1, 1, FALSE},
    {"power", 1, 1, TRUE},
    {"toggle", 0, 0, TRUE},
    {"discoverable", 1, 2, TRUE},
    {"pairable", 1, 1, TRUE},
    {"scan", 1, 2, TRUE},
    {"scan-worker", 2, 2, TRUE},
    {"pair", 1, 1, TRUE},
    {"connect", 1, 1, TRUE},
    {"disconnect", 1, 1, TRUE},
    {"trust", 1, 1, TRUE},
    {"untrust", 1, 1, TRUE},
    {"block", 1, 1, TRUE},
    {"unblock", 1, 1, TRUE},
    {"remove", 1, 1, TRUE},
    {"controller-select", 1, 1, TRUE},
    {"audio-profile", 2, 2, TRUE},
    {"audio-route", 2, 2, TRUE},
    {"cancel-pairing", 1, 1, TRUE},
    {"device-alias", 2, 2, TRUE},
    {"device-alias-reset", 1, 1, TRUE},
    {"controller-alias", 1, 1, TRUE},
    {"controller-alias-reset", 0, 0, TRUE},
};
const BackendService anto_service_bluetooth = {"bluetooth", operations, G_N_ELEMENTS(operations), anto_bluetooth_execute};
int backend_bluetooth_main(int argc, char **argv) {
    return backend_service_call(&anto_service_bluetooth, argc, argv);
}
