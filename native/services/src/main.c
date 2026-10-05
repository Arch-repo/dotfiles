#include "service.h"
#include <locale.h>
int main(int argc, char **argv) {
    setlocale(LC_ALL, "");
    if (argc == 2 && g_strcmp0(argv[1], "--describe") == 0) { backend_service_describe(); return 0; }
    g_autofree char *program = g_path_get_basename(argv[0]);
    if (g_strcmp0(program, "anto-config") == 0)
        return backend_service_call(backend_service_find("config"), argc - 1, argv + 1);
    if (argc < 3) return backend_usage(NULL, "SERVIZIO OPERAZIONE [ARGOMENTI]");
    const BackendService *service = backend_service_find(argv[1]);
    if (!service) return backend_error(2, "invalid-domain", "Servizio non riconosciuto");
    return backend_service_call(service, argc - 2, argv + 2);
}
