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
  DYN_ERR_BINDING_FAILED = -9,
} dyn_result;

typedef struct dyn_vm dyn_vm;
typedef struct dyn_nio dyn_nio;
typedef struct dyn_eth_switch dyn_eth_switch;

typedef enum dyn_vm_status {
  DYN_VM_HALTED = 0,
  DYN_VM_SHUTDOWN = 1,
  DYN_VM_RUNNING = 2,
  DYN_VM_SUSPENDED = 3,
} dyn_vm_status;

typedef struct dyn_nio_stats {
  uint64_t packets_in;
  uint64_t packets_out;
  uint64_t bytes_in;
  uint64_t bytes_out;
} dyn_nio_stats;

const char *dyn_result_message(dyn_result result);

dyn_result dyn_runtime_init(int argc, char *argv[]);
void dyn_runtime_shutdown(void);
int dyn_runtime_is_initialized(void);

dyn_result dyn_vm_create(const char *name, int32_t instance_id,
                         const char *platform, dyn_vm **out_vm);
void dyn_vm_release(dyn_vm *vm);
dyn_result dyn_vm_start(dyn_vm *vm);
dyn_result dyn_vm_stop(dyn_vm *vm);
dyn_result dyn_vm_get_status(const dyn_vm *vm, dyn_vm_status *out_status);
dyn_result dyn_vm_set_ram(dyn_vm *vm, uint32_t megabytes);
dyn_result dyn_vm_attach_nio(dyn_vm *vm, uint32_t slot, uint32_t port,
                             dyn_nio *nio);

dyn_result dyn_nio_create_udp(const char *name, uint16_t local_port,
                              const char *remote_host, uint16_t remote_port,
                              dyn_nio **out_nio);
dyn_result dyn_nio_create_udp_auto(const char *name, const char *local_addr,
                                   uint16_t port_start, uint16_t port_end,
                                   dyn_nio **out_nio, uint16_t *out_local_port);
void dyn_nio_release(dyn_nio *nio);
dyn_result dyn_nio_get_stats(const dyn_nio *nio, dyn_nio_stats *out_stats);

dyn_result dyn_eth_switch_create(const char *name, dyn_eth_switch **out_switch);
void dyn_eth_switch_release(dyn_eth_switch *sw);
dyn_result dyn_eth_switch_add_nio(dyn_eth_switch *sw, dyn_nio *nio);

#ifdef __cplusplus
}
#endif

#endif
