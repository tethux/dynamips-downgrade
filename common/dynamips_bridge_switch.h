#ifndef DYNAMIPS_CORE_BRIDGE_SWITCH_H
#define DYNAMIPS_CORE_BRIDGE_SWITCH_H

#include "dynamips_bridge.h"

#ifdef __cplusplus
extern "C" {
#endif

dyn_core_eth_switch *dyn_core_eth_switch_create(const char *name);
void dyn_core_eth_switch_release(dyn_core_eth_switch *sw);
int dyn_core_eth_switch_delete(const char *name);
int dyn_core_eth_switch_add_nio(dyn_core_eth_switch *sw, dyn_core_nio *nio);

#ifdef __cplusplus
}
#endif

#endif
