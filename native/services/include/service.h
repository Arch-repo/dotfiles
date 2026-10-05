#pragma once
#include "backend.h"
typedef int (*BackendExecutor)(int argc, char **argv);
typedef struct { const char *name; guint min_args, max_args; gboolean mutation; } BackendOperation;
typedef struct { const char *name; const BackendOperation *operations; guint count; BackendExecutor execute; } BackendService;
int backend_service_call(const BackendService *service, int argc, char **argv);
const BackendService *backend_service_find(const char *name);
void backend_service_describe(void);
int backend_operation_index(const BackendService *service, const char *name);
