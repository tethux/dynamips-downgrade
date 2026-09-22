#include "dynamips_bridge_vm.h"

#include "cpu.h"
#include "dev_c7200.h"
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

void dyn_core_vm_set_ram(dyn_core_vm *value, uint32_t megabytes) {
  vm_instance_t *vm = (vm_instance_t *)value;

  vm->ram_size = megabytes;
  if (vm->elf_machine_id == C7200_ELF_MACHINE_ID)
    VM_C7200(vm)->npe400_ram_size = vm->ram_size;
}

int dyn_core_vm_attach_nio(dyn_core_vm *vm, uint32_t slot, uint32_t port,
                           dyn_core_nio *endpoint) {
  netio_desc_t *nio = (netio_desc_t *)endpoint;
  return vm_slot_add_nio_binding((vm_instance_t *)vm, slot, port, nio->name);
}

int dyn_core_vm_extract_config(dyn_core_vm *value, dyn_bytes *out_startup,
                               dyn_bytes *out_private) {
  vm_instance_t *vm = (vm_instance_t *)value;

  if (vm->platform->nvram_extract_config == NULL)
    return (-2);
  return vm->platform->nvram_extract_config(
             vm, &out_startup->data, &out_startup->size, &out_private->data,
             &out_private->size) == 0
             ? 0
             : -1;
}
