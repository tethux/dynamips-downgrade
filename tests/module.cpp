#include <dynamips/dynamips.h>

import dynamips;

int main() {
  dyn_vm_status status = DYN_VM_RUNNING;
  if (dyn_vm_get_status(nullptr, &status) != DYN_ERR_INVALID_ARGUMENT ||
      status != DYN_VM_HALTED)
    return 1;
  dyn_nio *nio = reinterpret_cast<dyn_nio *>(1);
  if (dyn_nio_create_udp_auto("test", "127.0.0.1", 0, 0, &nio, nullptr) !=
          DYN_ERR_INVALID_ARGUMENT ||
      nio != nullptr)
    return 1;
  return dynamips::message(dynamips::error::internal).empty() ? 1 : 0;
}
