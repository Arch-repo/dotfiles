#include "service.h"
#define SERVICE(name) extern const BackendService anto_service_##name;
SERVICE(keyboard) SERVICE(floating) SERVICE(background) SERVICE(notifications)
SERVICE(audio) SERVICE(bluetooth) SERVICE(calendar) SERVICE(capture) SERVICE(config)
SERVICE(display) SERVICE(energy) SERVICE(network) SERVICE(notes) SERVICE(record) SERVICE(session) SERVICE(system)
static const BackendService *const services[] = {
    &anto_service_keyboard, &anto_service_floating, &anto_service_background, &anto_service_notifications,
    &anto_service_audio, &anto_service_bluetooth, &anto_service_calendar, &anto_service_capture,
    &anto_service_config, &anto_service_display, &anto_service_energy, &anto_service_network,
    &anto_service_notes, &anto_service_record, &anto_service_session, &anto_service_system,
};
const BackendService *backend_service_find(const char *name) {
    for (guint i = 0; i < G_N_ELEMENTS(services); i++)
        if (g_strcmp0(services[i]->name, name) == 0) return services[i];
    return NULL;
}
int backend_service_call(const BackendService *service, int argc, char **argv) {
    if (!service || argc < 1) return backend_usage(service ? service->name : NULL, "OPERAZIONE [ARGOMENTI]");
    for (guint i = 0; i < service->count; i++) {
        const BackendOperation *operation = &service->operations[i];
        if (g_strcmp0(operation->name, argv[0]) != 0) continue;
        guint arguments = argc - 1;
        if (arguments < operation->min_args || arguments > operation->max_args)
            return backend_error(2, "invalid-arguments", "Numero di argomenti non valido");
        return service->execute(argc, argv);
    }
    return backend_error(2, "invalid-operation", "Operazione non disponibile per questo servizio");
}
void backend_service_describe(void) {
    json_object *root = json_object_new_object();
    json_object_object_add(root, "version", json_object_new_int(1));
    json_object *modules = json_object_new_object();
    json_object_object_add(root, "services", modules);
    for (guint i = 0; i < G_N_ELEMENTS(services); i++) {
        const BackendService *service = services[i];
        json_object *operations = json_object_new_array();
        json_object_object_add(modules, service->name, operations);
        for (guint j = 0; j < service->count; j++) {
            const BackendOperation *operation = &service->operations[j];
            json_object *item = json_object_new_object();
            json_object_object_add(item, "name", json_object_new_string(operation->name));
            json_object_object_add(item, "minArgs", json_object_new_int(operation->min_args));
            json_object_object_add(item, "maxArgs", json_object_new_int(operation->max_args));
            json_object_object_add(item, "mutation", json_object_new_boolean(operation->mutation));
            json_object_array_add(operations, item);
        }
    }
    g_print("%s\n", json_object_to_json_string_ext(root, JSON_C_TO_STRING_PRETTY));
    json_object_put(root);
}
int backend_operation_index(const BackendService *service, const char *name) {
    for (guint i = 0; i < service->count; i++)
        if (g_strcmp0(service->operations[i].name, name) == 0) return (int)i;
    return -1;
}
