#include "dynamips_bridge.h"

#include "cpu.h"
#include "vm.h"
#include "net_io.h"

dyn_core_vm *dyn_core_vm_create(const char *name, int32_t instance_id,
                                const char *platform) {
  vm_instance_t *vm;

  vm = vm_create_instance((char *)name, instance_id, (char *)platform);
  if (vm == NULL)
    return (NULL);

  vm->vtty_con_type = 0;
  vm->vtty_aux_type = 0;
  return ((dyn_core_vm *)vm);
}

void dyn_core_vm_release(dyn_core_vm *vm) { vm_release((vm_instance_t *)vm); }

int dyn_core_vm_delete(const char *name) {
  return (vm_delete_instance((char *)name));
}

int dyn_core_vm_start(dyn_core_vm *vm) {
  return (vm_init_instance((vm_instance_t *)vm));
}

int dyn_core_vm_stop(dyn_core_vm *vm) {
  return (vm_stop_instance((vm_instance_t *)vm));
}

int dyn_core_vm_get_status(const dyn_core_vm *vm, dyn_vm_status *out_status) {
  switch (((const vm_instance_t *)vm)->status) {
  case VM_STATUS_HALTED:
    *out_status = DYN_VM_HALTED;
    return (0);
  case VM_STATUS_SHUTDOWN:
    *out_status = DYN_VM_SHUTDOWN;
    return (0);
  case VM_STATUS_RUNNING:
    *out_status = DYN_VM_RUNNING;
    return (0);
  case VM_STATUS_SUSPENDED:
    *out_status = DYN_VM_SUSPENDED;
    return (0);
  }
  return (-1);
}

dyn_core_nio *dyn_core_nio_create_udp(const char *name, uint16_t local_port,
                                      const char *remote_host,
                                      uint16_t remote_port) {
  return ((dyn_core_nio *)netio_desc_create_udp(
      (char *)name, local_port, (char *)remote_host, remote_port));
}

void dyn_core_nio_release(dyn_core_nio *nio) {
  netio_desc_t *value = (netio_desc_t *)nio;

  netio_release(value->name);
}

int dyn_core_nio_delete(const char *name) {
  return (netio_delete((char *)name));
}
