#ifndef DYNAMIPS_PUBLIC_PLATFORM_H
#define DYNAMIPS_PUBLIC_PLATFORM_H

#include "result.h"
#include "vm.h"

#ifdef __cplusplus
extern "C" {
#endif

dyn_result dyn_c7200_set_npe(dyn_vm *vm, const char *npe_type);
dyn_result dyn_c7200_set_midplane(dyn_vm *vm, const char *midplane_type);
dyn_result dyn_c7200_set_mac_addr(dyn_vm *vm, const uint8_t mac[6]);

#ifdef __cplusplus
}
#endif

#endif
