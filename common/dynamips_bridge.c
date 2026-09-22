#include "dynamips_bridge.h"

#include "cpu.h"
#include "dev_c7200.h"
#include "eth_switch.h"
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

dyn_core_nio *dyn_core_nio_create_udp(const char *name, uint16_t local_port,
                                      const char *remote_host,
                                      uint16_t remote_port) {
  return ((dyn_core_nio *)netio_desc_create_udp(
      (char *)name, local_port, (char *)remote_host, remote_port));
}

dyn_core_nio *dyn_core_nio_create_udp_auto(const char *name,
                                           const char *local_addr,
                                           uint16_t port_start,
                                           uint16_t port_end) {
  return ((dyn_core_nio *)netio_desc_create_udp_auto(
      (char *)name, (char *)local_addr, port_start, port_end));
}

int dyn_core_nio_udp_auto_local_port(const dyn_core_nio *nio) {
  return netio_udp_auto_get_local_port((netio_desc_t *)nio);
}

void dyn_core_nio_release(dyn_core_nio *nio) {
  netio_desc_t *value = (netio_desc_t *)nio;

  netio_release(value->name);
}

int dyn_core_nio_delete(const char *name) {
  return (netio_delete((char *)name));
}

void dyn_core_nio_get_stats(const dyn_core_nio *value,
                            dyn_nio_stats *out_stats) {
  const netio_desc_t *nio = (const netio_desc_t *)value;

  out_stats->packets_in = nio->stats_pkts_in;
  out_stats->packets_out = nio->stats_pkts_out;
  out_stats->bytes_in = nio->stats_bytes_in;
  out_stats->bytes_out = nio->stats_bytes_out;
}

dyn_core_eth_switch *dyn_core_eth_switch_create(const char *name) {
  return (dyn_core_eth_switch *)ethsw_create((char *)name);
}

void dyn_core_eth_switch_release(dyn_core_eth_switch *value) {
  ethsw_table_t *sw = (ethsw_table_t *)value;
  ethsw_release(sw->name);
}

int dyn_core_eth_switch_delete(const char *name) {
  return ethsw_delete((char *)name);
}

int dyn_core_eth_switch_add_nio(dyn_core_eth_switch *value,
                                dyn_core_nio *endpoint) {
  ethsw_table_t *sw = (ethsw_table_t *)value;
  netio_desc_t *nio = (netio_desc_t *)endpoint;
  return ethsw_add_netio(sw, nio->name);
}
