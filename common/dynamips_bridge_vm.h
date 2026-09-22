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
int dyn_core_vm_delete_handle(dyn_core_vm **vm);
int dyn_core_vm_suspend(dyn_core_vm *vm);
int dyn_core_vm_resume(dyn_core_vm *vm);
int dyn_core_vm_set_ios(dyn_core_vm *vm, const char *path);
void dyn_core_vm_set_nvram(dyn_core_vm *vm, uint32_t kilobytes);
void dyn_core_vm_set_sparse_mem(dyn_core_vm *vm, int enabled);
void dyn_core_vm_set_conf_reg(dyn_core_vm *vm, uint32_t value);
void dyn_core_vm_set_idle_pc(dyn_core_vm *vm, uint64_t value);
void dyn_core_vm_set_con_tcp_port(dyn_core_vm *vm, uint16_t port);
int dyn_core_vm_push_config(dyn_core_vm *vm, const uint8_t *startup,
                            size_t startup_size, const uint8_t *private_config,
                            size_t private_size);
int dyn_core_vm_start(dyn_core_vm *vm);
int dyn_core_vm_stop(dyn_core_vm *vm);
int dyn_core_vm_get_status(const dyn_core_vm *vm, dyn_vm_status *out_status);
void dyn_core_vm_set_ram(dyn_core_vm *vm, uint32_t megabytes);
int dyn_core_vm_add_card(dyn_core_vm *vm, uint32_t slot, const char *card);
int dyn_core_vm_remove_card(dyn_core_vm *vm, uint32_t slot);
int dyn_core_vm_attach_nio(dyn_core_vm *vm, uint32_t slot, uint32_t port,
                           dyn_core_nio *nio);
int dyn_core_vm_detach_nio(dyn_core_vm *vm, uint32_t slot, uint32_t port);
int dyn_core_vm_extract_config(dyn_core_vm *vm, dyn_bytes *out_startup,
                               dyn_bytes *out_private);

#ifdef __cplusplus
}
#endif

#endif
