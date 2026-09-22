#include "dynamips_bridge_vm.h"

#include "cpu.h"
#include "dev_c7200.h"
#include "vm.h"
#include "net_io.h"
#include "registry.h"
#include "dev_vtty.h"
#include "cisco_card.h"

#include <stdlib.h>
#include <string.h>

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

int dyn_core_vm_delete_handle(dyn_core_vm **value) {
  vm_instance_t *vm = (vm_instance_t *)*value;
  char *name = strdup(vm->name);
  int result;
  if (name == NULL)
    return (-2);
  vm_release(vm);
  *value = NULL;
  result = vm_delete_instance(name);
  if (result == 1 && registry_exists(name, OBJ_TYPE_VM) == NULL) {
    free(name);
    return (0);
  }
  *value = (dyn_core_vm *)vm_acquire(name);
  free(name);
  return (-1);
}

int dyn_core_vm_suspend(dyn_core_vm *value) {
  return vm_suspend((vm_instance_t *)value);
}

int dyn_core_vm_resume(dyn_core_vm *value) {
  return vm_resume((vm_instance_t *)value);
}

int dyn_core_vm_set_ios(dyn_core_vm *value, const char *path) {
  return vm_ios_set_image((vm_instance_t *)value, (char *)path);
}

void dyn_core_vm_set_nvram(dyn_core_vm *value, uint32_t kilobytes) {
  ((vm_instance_t *)value)->nvram_size = kilobytes;
}

void dyn_core_vm_set_sparse_mem(dyn_core_vm *value, int enabled) {
  ((vm_instance_t *)value)->sparse_mem = enabled;
}

void dyn_core_vm_set_conf_reg(dyn_core_vm *vm, uint32_t value) {
  ((vm_instance_t *)vm)->conf_reg_setup = value;
}

void dyn_core_vm_set_idle_pc(dyn_core_vm *vm, uint64_t value) {
  ((vm_instance_t *)vm)->idle_pc = value;
}

void dyn_core_vm_set_con_tcp_port(dyn_core_vm *value, uint16_t port) {
  vm_instance_t *vm = (vm_instance_t *)value;
  vm->vtty_con_type = VTTY_TYPE_TCP;
  vm->vtty_con_tcp_port = port;
}

int dyn_core_vm_push_config(dyn_core_vm *value, const uint8_t *startup,
                            size_t startup_size, const uint8_t *private_config,
                            size_t private_size) {
  vm_instance_t *vm = (vm_instance_t *)value;
  if (vm->platform->nvram_push_config == NULL)
    return (-2);
  return vm->platform->nvram_push_config(vm, (u_char *)startup, startup_size,
                                         (u_char *)private_config,
                                         private_size);
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

int dyn_core_vm_add_card(dyn_core_vm *vm, uint32_t slot, const char *card) {
  return vm_slot_add_binding((vm_instance_t *)vm, (char *)card, slot, 0);
}

int dyn_core_vm_remove_card(dyn_core_vm *vm, uint32_t slot) {
  return vm_slot_remove_binding((vm_instance_t *)vm, slot, 0);
}

int dyn_core_vm_detach_nio(dyn_core_vm *vm, uint32_t slot, uint32_t port) {
  return vm_slot_remove_nio_binding((vm_instance_t *)vm, slot, port);
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
