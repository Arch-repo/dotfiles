#include "service.h"
/* Application boundary: validate before accessing a provider or starting a device command. */
int anto_display_execute(int argc, char **argv);
static const BackendOperation operations[] = {
    {"status", 0, 0, FALSE},
    {"list", 0, 0, FALSE},
    {"modes", 1, 1, FALSE},
    {"enable", 1, 1, TRUE},
    {"disable", 1, 1, TRUE},
    {"toggle", 1, 1, TRUE},
    {"only", 1, 1, TRUE},
    {"only-primary", 0, 0, TRUE},
    {"only-external", 0, 0, TRUE},
    {"extend", 0, 1, TRUE},
    {"mirror", 0, 1, TRUE},
    {"duplicate", 0, 1, TRUE},
    {"scale", 2, 2, TRUE},
    {"transform", 2, 2, TRUE},
    {"mode", 2, 2, TRUE},
    {"position", 2, 2, TRUE},
    {"arrange", 1, 1, TRUE},
    {"focus", 1, 1, TRUE},
    {"dpms-on", 0, 1, TRUE},
    {"dpms-off", 0, 1, TRUE},
    {"persistent-preview", 0, 0, FALSE},
    {"persist-current", 0, 0, TRUE},
    {"profile-save", 0, 1, TRUE},
    {"profile-apply", 0, 1, TRUE},
    {"profile-list", 0, 0, FALSE},
    {"profile-delete", 1, 1, TRUE},
    {"editor", 0, 0, TRUE},
    {"help", 0, 0, FALSE},
    {"-h", 0, 0, FALSE},
    {"--help", 0, 0, FALSE},
};
const BackendService anto_service_display = {"display", operations, G_N_ELEMENTS(operations), anto_display_execute};
int backend_display_main(int argc, char **argv) {
    return backend_service_call(&anto_service_display, argc, argv);
}
