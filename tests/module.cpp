#include <dynamips/dynamips.h>

import dynamips;

int main() {
  dyn_vm_status status = DYN_VM_RUNNING;
  if (dyn_vm_get_status(nullptr, &status) != DYN_ERR_INVALID_ARGUMENT ||
      status != DYN_VM_HALTED)
    return 1;
  return dynamips::message(dynamips::error::internal).empty() ? 1 : 0;
}
