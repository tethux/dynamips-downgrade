#include "dynamips_bridge_switch.h"

#include "eth_switch.h"
#include "net_io.h"

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
