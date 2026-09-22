#ifndef DYNAMIPS_CORE_BRIDGE_VM_H
#define DYNAMIPS_CORE_BRIDGE_VM_H

#include "dynamips_bridge.h"

#ifdef __cplusplus
extern "C" {
#endif

dyn_core_vm *dyn_core_vm_create(const char *name, int32_t instance_id,
                                const char *platform);
void dyn_core_vm_release(dyn_core_vm *vm);
int dyn_core_vm_delete(const char *name);
int dyn_core_vm_start(dyn_core_vm *vm);
int dyn_core_vm_stop(dyn_core_vm *vm);
int dyn_core_vm_get_status(const dyn_core_vm *vm, dyn_vm_status *out_status);
void dyn_core_vm_set_ram(dyn_core_vm *vm, uint32_t megabytes);
int dyn_core_vm_attach_nio(dyn_core_vm *vm, uint32_t slot, uint32_t port,
                           dyn_core_nio *nio);
int dyn_core_vm_extract_config(dyn_core_vm *vm, dyn_bytes *out_startup,
                               dyn_bytes *out_private);

#ifdef __cplusplus
}
#endif

#endif
