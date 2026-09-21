#ifndef DYNAMIPS_CORE_BRIDGE_H
#define DYNAMIPS_CORE_BRIDGE_H

#include <stdint.h>
#include <dynamips/dynamips.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct dyn_core_vm dyn_core_vm;
typedef struct dyn_core_nio dyn_core_nio;

dyn_core_vm *dyn_core_vm_create(const char *name, int32_t instance_id,
                                const char *platform);
void dyn_core_vm_release(dyn_core_vm *vm);
int dyn_core_vm_delete(const char *name);
int dyn_core_vm_start(dyn_core_vm *vm);
int dyn_core_vm_stop(dyn_core_vm *vm);
int dyn_core_vm_get_status(const dyn_core_vm *vm, dyn_vm_status *out_status);

dyn_core_nio *dyn_core_nio_create_udp(const char *name, uint16_t local_port,
                                      const char *remote_host,
                                      uint16_t remote_port);
void dyn_core_nio_release(dyn_core_nio *nio);
int dyn_core_nio_delete(const char *name);

#ifdef __cplusplus
}
#endif

#endif
