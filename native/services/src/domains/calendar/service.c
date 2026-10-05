#include "service.h"
/* Application boundary: validate before accessing a provider or starting a device command. */
int anto_calendar_execute(int argc, char **argv);
static const BackendOperation operations[] = {
    {"list", 0, 0, FALSE},
    {"add", 0, 0, TRUE},
    {"delete", 1, 1, TRUE},
    {"sync", 0, 0, TRUE},
};
const BackendService anto_service_calendar = {"calendar", operations, G_N_ELEMENTS(operations), anto_calendar_execute};
int backend_calendar_main(int argc, char **argv) {
    return backend_service_call(&anto_service_calendar, argc, argv);
}
