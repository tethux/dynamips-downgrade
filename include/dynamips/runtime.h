#ifndef DYNAMIPS_PUBLIC_RUNTIME_H
#define DYNAMIPS_PUBLIC_RUNTIME_H

#include "result.h"

#ifdef __cplusplus
extern "C" {
#endif

dyn_result dyn_runtime_init(int argc, char *argv[]);
dyn_result dyn_runtime_init_embedded(void);
void dyn_runtime_shutdown(void);
int dyn_runtime_is_initialized(void);

#ifdef __cplusplus
}
#endif

#endif
