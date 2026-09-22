#include "api_internal.h"
#include "dynamips_bridge_platform.h"

namespace {

[[nodiscard]] fn platform_result(int value) noexcept -> dyn_result {
  if (value == 0)
    return DYN_OK;
  return value == -2 ? DYN_ERR_UNSUPPORTED : DYN_ERR_INVALID_STATE;
}

} // namespace

cfn dyn_c7200_set_npe(dyn_vm *vm, const char *npe_type) -> dyn_result {
  try {
    if (vm == nullptr || vm->value == nullptr || dyn_invalid_text(npe_type))
      return DYN_ERR_INVALID_ARGUMENT;
    if (const let status = dyn_require_runtime(); status != DYN_OK)
      return status;
    return platform_result(dyn_core_c7200_set_npe(vm->value, npe_type));
  } catch (...) {
    return DYN_ERR_INTERNAL;
  }
}

cfn dyn_c7200_set_midplane(dyn_vm *vm, const char *midplane_type)
    -> dyn_result {
  try {
    if (vm == nullptr || vm->value == nullptr ||
        dyn_invalid_text(midplane_type))
      return DYN_ERR_INVALID_ARGUMENT;
    if (const let status = dyn_require_runtime(); status != DYN_OK)
      return status;
    return platform_result(
        dyn_core_c7200_set_midplane(vm->value, midplane_type));
  } catch (...) {
    return DYN_ERR_INTERNAL;
  }
}

cfn dyn_c7200_set_mac_addr(dyn_vm *vm, const uint8_t mac[6]) -> dyn_result {
  try {
    if (vm == nullptr || vm->value == nullptr || mac == nullptr)
      return DYN_ERR_INVALID_ARGUMENT;
    if (const let status = dyn_require_runtime(); status != DYN_OK)
      return status;
    return platform_result(dyn_core_c7200_set_mac_addr(vm->value, mac));
  } catch (...) {
    return DYN_ERR_INTERNAL;
  }
}
