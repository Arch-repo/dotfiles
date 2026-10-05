#include "service.h"
/* Application boundary: validate before accessing a provider or starting a device command. */
int anto_capture_execute(int argc, char **argv);
static const BackendOperation operations[] = {
    {"area", 0, 0, TRUE},
    {"window", 0, 0, TRUE},
    {"monitor", 0, 0, TRUE},
    {"clipboard-area", 0, 0, TRUE},
    {"ocr", 0, 0, TRUE},
    {"open", 0, 0, TRUE},
};
const BackendService anto_service_capture = {"capture", operations, G_N_ELEMENTS(operations), anto_capture_execute};
int backend_capture_main(int argc, char **argv) {
    return backend_service_call(&anto_service_capture, argc, argv);
}
