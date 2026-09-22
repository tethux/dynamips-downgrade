#ifndef DYNAMIPS_PUBLIC_NIO_H
#define DYNAMIPS_PUBLIC_NIO_H

#include "result.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct dyn_nio dyn_nio;

typedef struct dyn_nio_stats {
  uint64_t packets_in;
  uint64_t packets_out;
  uint64_t bytes_in;
  uint64_t bytes_out;
} dyn_nio_stats;

typedef enum dyn_filter_direction {
  DYN_FILTER_RX = 0,
  DYN_FILTER_TX = 1,
  DYN_FILTER_BOTH = 2,
} dyn_filter_direction;

dyn_result dyn_nio_create_udp(const char *name, uint16_t local_port,
                              const char *remote_host, uint16_t remote_port,
                              dyn_nio **out_nio);
dyn_result dyn_nio_create_udp_auto(const char *name, const char *local_addr,
                                   uint16_t port_start, uint16_t port_end,
                                   dyn_nio **out_nio, uint16_t *out_local_port);
void dyn_nio_release(dyn_nio *nio);
dyn_result dyn_nio_get_stats(const dyn_nio *nio, dyn_nio_stats *out_stats);
dyn_result dyn_nio_setup_filter(dyn_nio *nio, dyn_filter_direction direction,
                                size_t option_count,
                                const char *const options[]);

#ifdef __cplusplus
}
#endif

#endif
