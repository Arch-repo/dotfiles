#include "service.h"
/* Application boundary: validate before accessing a provider or starting a device command. */
int anto_audio_execute(int argc, char **argv);
static const BackendOperation operations[] = {
    {"snapshot", 0, 0, FALSE},
    {"volume", 2, 2, TRUE},
    {"mute-toggle", 1, 1, TRUE},
    {"player", 1, 1, TRUE},
    {"mixer", 0, 0, TRUE},
};
const BackendService anto_service_audio = {"audio", operations, G_N_ELEMENTS(operations), anto_audio_execute};
int backend_audio_main(int argc, char **argv) {
    return backend_service_call(&anto_service_audio, argc, argv);
}
