#ifndef DYNAMIPS_BINDINGS_API_INTERNAL_H
#define DYNAMIPS_BINDINGS_API_INTERNAL_H

#include <dynamips/dynamips.h>
#include <dynamips/macros.h>

#include "dynamips_bridge.h"
#include "dynamips_bridge_vm.h"
#include "dynamips_bridge_nio.h"
#include "dynamips_bridge_switch.h"

struct dyn_vm {
  dyn_core_vm *value;
};

struct dyn_nio {
  dyn_core_nio *value;
};

struct dyn_eth_switch {
  dyn_core_eth_switch *value;
};

[[nodiscard]] inline fn dyn_invalid_text(const char *value) noexcept -> bool {
  return value == nullptr || value[0] == '\0';
}

[[nodiscard]] inline fn dyn_require_runtime() noexcept -> dyn_result {
  return dyn_runtime_is_initialized() ? DYN_OK : DYN_ERR_NOT_INITIALIZED;
}

#endif
