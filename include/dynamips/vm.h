#ifndef DYNAMIPS_PUBLIC_VM_H
#define DYNAMIPS_PUBLIC_VM_H

#include "result.h"
#include "nio.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct dyn_vm dyn_vm;

typedef enum dyn_vm_status {
  DYN_VM_HALTED = 0,
  DYN_VM_SHUTDOWN = 1,
  DYN_VM_RUNNING = 2,
  DYN_VM_SUSPENDED = 3,
} dyn_vm_status;

dyn_result dyn_vm_create(const char *name, int32_t instance_id,
                         const char *platform, dyn_vm **out_vm);
void dyn_vm_release(dyn_vm *vm);
dyn_result dyn_vm_delete(dyn_vm **vm);
dyn_result dyn_vm_suspend(dyn_vm *vm);
dyn_result dyn_vm_resume(dyn_vm *vm);
dyn_result dyn_vm_set_ios(dyn_vm *vm, const char *path);
dyn_result dyn_vm_set_nvram(dyn_vm *vm, uint32_t kilobytes);
dyn_result dyn_vm_set_sparse_mem(dyn_vm *vm, int enabled);
dyn_result dyn_vm_set_conf_reg(dyn_vm *vm, uint32_t value);
dyn_result dyn_vm_set_idle_pc(dyn_vm *vm, uint64_t value);
dyn_result dyn_vm_set_con_tcp_port(dyn_vm *vm, uint16_t port);
dyn_result dyn_vm_push_config(dyn_vm *vm, const uint8_t *startup,
                              size_t startup_size,
                              const uint8_t *private_config,
                              size_t private_size);
dyn_result dyn_vm_start(dyn_vm *vm);
dyn_result dyn_vm_stop(dyn_vm *vm);
dyn_result dyn_vm_get_status(const dyn_vm *vm, dyn_vm_status *out_status);
dyn_result dyn_vm_set_ram(dyn_vm *vm, uint32_t megabytes);
dyn_result dyn_vm_add_card(dyn_vm *vm, uint32_t slot, const char *card);
dyn_result dyn_vm_remove_card(dyn_vm *vm, uint32_t slot);
dyn_result dyn_vm_attach_nio(dyn_vm *vm, uint32_t slot, uint32_t port,
                             dyn_nio *nio);
dyn_result dyn_vm_detach_nio(dyn_vm *vm, uint32_t slot, uint32_t port);
dyn_result dyn_vm_extract_config(dyn_vm *vm, dyn_bytes *out_startup,
                                 dyn_bytes *out_private);

#ifdef __cplusplus
}
#endif

#endif
