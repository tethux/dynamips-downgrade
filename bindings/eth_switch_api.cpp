#include "api_internal.h"

#include <new>

cfn dyn_eth_switch_create(const char *name, dyn_eth_switch **out_switch)
    -> dyn_result {
  try {
    if (out_switch == nullptr)
      return DYN_ERR_INVALID_ARGUMENT;
    *out_switch = nullptr;
    if (dyn_invalid_text(name))
      return DYN_ERR_INVALID_ARGUMENT;
    if (const let status = dyn_require_runtime(); status != DYN_OK)
      return status;
    let *value = dyn_core_eth_switch_create(name);
    if (value == nullptr)
      return DYN_ERR_CREATE_FAILED;
    let *handle = new (std::nothrow) dyn_eth_switch{value};
    if (handle == nullptr) {
      dyn_core_eth_switch_release(value);
      dyn_core_eth_switch_delete(name);
      return DYN_ERR_OUT_OF_MEMORY;
    }
    *out_switch = handle;
    return DYN_OK;
  } catch (const std::bad_alloc &) {
    return DYN_ERR_OUT_OF_MEMORY;
  } catch (...) {
    return DYN_ERR_INTERNAL;
  }
}

cfn dyn_eth_switch_release(dyn_eth_switch *sw) -> void {
  if (sw == nullptr)
    return;
  dyn_core_eth_switch_release(sw->value);
  delete sw;
}

cfn dyn_eth_switch_add_nio(dyn_eth_switch *sw, dyn_nio *nio) -> dyn_result {
  try {
    if (sw == nullptr || sw->value == nullptr || nio == nullptr ||
        nio->value == nullptr)
      return DYN_ERR_INVALID_ARGUMENT;
    if (const let status = dyn_require_runtime(); status != DYN_OK)
      return status;
    return dyn_core_eth_switch_add_nio(sw->value, nio->value) == 0
               ? DYN_OK
               : DYN_ERR_BINDING_FAILED;
  } catch (...) {
    return DYN_ERR_INTERNAL;
  }
}
