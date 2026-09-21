#ifndef DYNAMIPS_PUBLIC_DYNAMIPS_H
#define DYNAMIPS_PUBLIC_DYNAMIPS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum dyn_result {
  DYN_OK = 0,
  DYN_ERR_INVALID_ARGUMENT = -1,
  DYN_ERR_NOT_INITIALIZED = -2,
  DYN_ERR_ALREADY_INITIALIZED = -3,
  DYN_ERR_OUT_OF_MEMORY = -4,
  DYN_ERR_CREATE_FAILED = -5,
  DYN_ERR_START_FAILED = -6,
  DYN_ERR_STOP_FAILED = -7,
  DYN_ERR_INTERNAL = -8,
} dyn_result;

typedef struct dyn_vm dyn_vm;
typedef struct dyn_nio dyn_nio;

const char *dyn_result_message(dyn_result result);

dyn_result dyn_runtime_init(int argc, char *argv[]);
void dyn_runtime_shutdown(void);
int dyn_runtime_is_initialized(void);

dyn_result dyn_vm_create(const char *name, int32_t instance_id,
                         const char *platform, dyn_vm **out_vm);
void dyn_vm_release(dyn_vm *vm);
dyn_result dyn_vm_start(dyn_vm *vm);
dyn_result dyn_vm_stop(dyn_vm *vm);

dyn_result dyn_nio_create_udp(const char *name, uint16_t local_port,
                              const char *remote_host, uint16_t remote_port,
                              dyn_nio **out_nio);
void dyn_nio_release(dyn_nio *nio);

#ifdef __cplusplus
}
#endif

#endif
