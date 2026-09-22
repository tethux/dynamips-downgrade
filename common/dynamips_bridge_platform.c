#include "dynamips_bridge_platform.h"

#include <stdio.h>
#include <string.h>

#include "dev_c7200.h"
#include "vm.h"

static c7200_t *c7200_from_vm(dyn_core_vm *value) {
  vm_instance_t *vm = (vm_instance_t *)value;

  if (vm->platform == NULL || strcmp(vm->platform->name, "c7200") != 0 ||
      vm->hw_data == NULL)
    return NULL;
  return VM_C7200(vm);
}

int dyn_core_c7200_set_npe(dyn_core_vm *value, const char *npe_type) {
  c7200_t *router = c7200_from_vm(value);

  if (router == NULL)
    return -2;
  return c7200_npe_set_type(router, (char *)npe_type);
}

int dyn_core_c7200_set_midplane(dyn_core_vm *value, const char *midplane_type) {
  c7200_t *router = c7200_from_vm(value);

  if (router == NULL)
    return -2;
  return c7200_midplane_set_type(router, (char *)midplane_type);
}

int dyn_core_c7200_set_mac_addr(dyn_core_vm *value, const uint8_t mac[6]) {
  c7200_t *router = c7200_from_vm(value);
  char text[18];

  if (router == NULL)
    return -2;
  snprintf(text, sizeof(text), "%02x:%02x:%02x:%02x:%02x:%02x", mac[0], mac[1],
           mac[2], mac[3], mac[4], mac[5]);
  return c7200_midplane_set_mac_addr(router, text);
}
