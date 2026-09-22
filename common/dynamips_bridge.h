#ifndef DYNAMIPS_CORE_BRIDGE_H
#define DYNAMIPS_CORE_BRIDGE_H

#include <stdint.h>
#include <dynamips/dynamips.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct dyn_core_vm dyn_core_vm;
typedef struct dyn_core_nio dyn_core_nio;
typedef struct dyn_core_eth_switch dyn_core_eth_switch;

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

dyn_core_nio *dyn_core_nio_create_udp(const char *name, uint16_t local_port,
                                      const char *remote_host,
                                      uint16_t remote_port);
dyn_core_nio *dyn_core_nio_create_udp_auto(const char *name,
                                           const char *local_addr,
                                           uint16_t port_start,
                                           uint16_t port_end);
int dyn_core_nio_udp_auto_local_port(const dyn_core_nio *nio);
void dyn_core_nio_release(dyn_core_nio *nio);
int dyn_core_nio_delete(const char *name);
void dyn_core_nio_get_stats(const dyn_core_nio *nio, dyn_nio_stats *out_stats);

dyn_core_eth_switch *dyn_core_eth_switch_create(const char *name);
void dyn_core_eth_switch_release(dyn_core_eth_switch *sw);
int dyn_core_eth_switch_delete(const char *name);
int dyn_core_eth_switch_add_nio(dyn_core_eth_switch *sw, dyn_core_nio *nio);

#ifdef __cplusplus
}
#endif

#endif
