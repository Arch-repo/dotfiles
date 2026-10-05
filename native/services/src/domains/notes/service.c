#include "service.h"
/* Application boundary: validate before accessing a provider or starting a device command. */
int anto_notes_execute(int argc, char **argv);
static const BackendOperation operations[] = {
    {"open", 0, 0, TRUE},
    {"init", 0, 0, TRUE},
    {"config-path", 0, 0, TRUE},
};
const BackendService anto_service_notes = {"notes", operations, G_N_ELEMENTS(operations), anto_notes_execute};
int backend_notes_main(int argc, char **argv) {
    return backend_service_call(&anto_service_notes, argc, argv);
}
