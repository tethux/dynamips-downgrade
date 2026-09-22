#include "dynamips_bridge_nio.h"

#include "net_io.h"
#include "net_io_filter.h"

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

int dyn_core_nio_setup_filter(dyn_core_nio *nio, int direction,
                              int option_count, char *options[]) {
  return netio_filter_setup((netio_desc_t *)nio, direction, option_count,
                            options);
}
