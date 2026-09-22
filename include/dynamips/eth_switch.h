#ifndef DYNAMIPS_PUBLIC_ETH_SWITCH_H
#define DYNAMIPS_PUBLIC_ETH_SWITCH_H

#include "result.h"
#include "nio.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct dyn_eth_switch dyn_eth_switch;

dyn_result dyn_eth_switch_create(const char *name, dyn_eth_switch **out_switch);
void dyn_eth_switch_release(dyn_eth_switch *sw);
dyn_result dyn_eth_switch_add_nio(dyn_eth_switch *sw, dyn_nio *nio);

#ifdef __cplusplus
}
#endif

#endif
