#ifndef DYNAMIPS_CORE_BRIDGE_NIO_H
#define DYNAMIPS_CORE_BRIDGE_NIO_H

#include "dynamips_bridge.h"

#ifdef __cplusplus
extern "C" {
#endif

dyn_core_nio *dyn_core_nio_create_udp(const char *name, uint16_t local_port,
                                      const char *remote_host,
                                      uint16_t remote_port);
dyn_core_nio *dyn_core_nio_create_udp_auto(const char *name,
                                           const char *local_addr,
                                           uint16_t port_start,
                                           uint16_t port_end);
dyn_core_nio *dyn_core_nio_create_tap(const char *name, const char *device);
int dyn_core_nio_connect_udp_auto(dyn_core_nio *nio, const char *host,
                                  uint16_t port);
int dyn_core_nio_delete_owned(dyn_core_nio *nio);
int dyn_core_nio_udp_auto_local_port(const dyn_core_nio *nio);
void dyn_core_nio_release(dyn_core_nio *nio);
int dyn_core_nio_delete(const char *name);
void dyn_core_nio_get_stats(const dyn_core_nio *nio, dyn_nio_stats *out_stats);
int dyn_core_nio_setup_filter(dyn_core_nio *nio, int direction,
                              int option_count, char *options[]);

#ifdef __cplusplus
}
#endif

#endif
