#ifndef DYNAMIPS_CORE_BRIDGE_PLATFORM_H
#define DYNAMIPS_CORE_BRIDGE_PLATFORM_H

#include "dynamips_bridge_vm.h"

#ifdef __cplusplus
extern "C" {
#endif

int dyn_core_c7200_set_npe(dyn_core_vm *vm, const char *npe_type);
int dyn_core_c7200_set_midplane(dyn_core_vm *vm, const char *midplane_type);
int dyn_core_c7200_set_mac_addr(dyn_core_vm *vm, const uint8_t mac[6]);

#ifdef __cplusplus
}
#endif

#endif
