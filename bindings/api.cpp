#include <dynamips/dynamips.h>
#include <dynamips/macros.h>

#include <new>

#include "dynamips_bridge.h"

struct dyn_vm {
  dyn_core_vm *value;
};

struct dyn_nio {
  dyn_core_nio *value;
};

struct dyn_eth_switch {
  dyn_core_eth_switch *value;
};

namespace {

[[nodiscard]] fn invalid_text(const char *value) noexcept -> bool {
  return value == nullptr || value[0] == '\0';
}

[[nodiscard]] fn require_runtime() noexcept -> dyn_result {
  return dyn_runtime_is_initialized() ? DYN_OK : DYN_ERR_NOT_INITIALIZED;
}

} // namespace

cfn dyn_vm_create(const char *name, int32_t instance_id, const char *platform,
                  dyn_vm **out_vm) -> dyn_result {
  try {
    if (out_vm == nullptr)
      return DYN_ERR_INVALID_ARGUMENT;
    *out_vm = nullptr;
    if (invalid_text(name) || invalid_text(platform))
      return DYN_ERR_INVALID_ARGUMENT;

    if (const let status = require_runtime(); status != DYN_OK)
      return status;

    let *value = dyn_core_vm_create(name, instance_id, platform);
    if (value == nullptr)
      return DYN_ERR_CREATE_FAILED;

    let *handle = new (std::nothrow) dyn_vm{value};
    if (handle == nullptr) {
      dyn_core_vm_release(value);
      dyn_core_vm_delete(name);
      return DYN_ERR_OUT_OF_MEMORY;
    }
    *out_vm = handle;
    return DYN_OK;
  } catch (const std::bad_alloc &) {
    if (out_vm != nullptr)
      *out_vm = nullptr;
    return DYN_ERR_OUT_OF_MEMORY;
  } catch (...) {
    if (out_vm != nullptr)
      *out_vm = nullptr;
    return DYN_ERR_INTERNAL;
  }
}

cfn dyn_vm_release(dyn_vm *vm) -> void {
  try {
    if (vm == nullptr)
      return;
    dyn_core_vm_release(vm->value);
    delete vm;
  } catch (...) {
  }
}

cfn dyn_vm_start(dyn_vm *vm) -> dyn_result {
  try {
    if (vm == nullptr || vm->value == nullptr)
      return DYN_ERR_INVALID_ARGUMENT;
    if (const let status = require_runtime(); status != DYN_OK)
      return status;
    return dyn_core_vm_start(vm->value) == 0 ? DYN_OK : DYN_ERR_START_FAILED;
  } catch (...) {
    return DYN_ERR_INTERNAL;
  }
}

cfn dyn_vm_stop(dyn_vm *vm) -> dyn_result {
  try {
    if (vm == nullptr || vm->value == nullptr)
      return DYN_ERR_INVALID_ARGUMENT;
    if (const let status = require_runtime(); status != DYN_OK)
      return status;
    return dyn_core_vm_stop(vm->value) == 0 ? DYN_OK : DYN_ERR_STOP_FAILED;
  } catch (...) {
    return DYN_ERR_INTERNAL;
  }
}

cfn dyn_vm_get_status(const dyn_vm *vm, dyn_vm_status *out_status)
    -> dyn_result {
  try {
    if (out_status == nullptr)
      return DYN_ERR_INVALID_ARGUMENT;
    *out_status = DYN_VM_HALTED;
    if (vm == nullptr || vm->value == nullptr)
      return DYN_ERR_INVALID_ARGUMENT;
    if (const let status = require_runtime(); status != DYN_OK)
      return status;
    return dyn_core_vm_get_status(vm->value, out_status) == 0
               ? DYN_OK
               : DYN_ERR_INTERNAL;
  } catch (...) {
    return DYN_ERR_INTERNAL;
  }
}

cfn dyn_vm_set_ram(dyn_vm *vm, uint32_t megabytes) -> dyn_result {
  try {
    if (vm == nullptr || vm->value == nullptr)
      return DYN_ERR_INVALID_ARGUMENT;
    if (const let status = require_runtime(); status != DYN_OK)
      return status;
    dyn_core_vm_set_ram(vm->value, megabytes);
    return DYN_OK;
  } catch (...) {
    return DYN_ERR_INTERNAL;
  }
}

cfn dyn_nio_create_udp(const char *name, uint16_t local_port,
                       const char *remote_host, uint16_t remote_port,
                       dyn_nio **out_nio) -> dyn_result {
  try {
    if (out_nio == nullptr)
      return DYN_ERR_INVALID_ARGUMENT;
    *out_nio = nullptr;
    if (invalid_text(name) || invalid_text(remote_host))
      return DYN_ERR_INVALID_ARGUMENT;

    if (const let status = require_runtime(); status != DYN_OK)
      return status;

    let *value =
        dyn_core_nio_create_udp(name, local_port, remote_host, remote_port);
    if (value == nullptr)
      return DYN_ERR_CREATE_FAILED;

    let *handle = new (std::nothrow) dyn_nio{value};
    if (handle == nullptr) {
      dyn_core_nio_release(value);
      dyn_core_nio_delete(name);
      return DYN_ERR_OUT_OF_MEMORY;
    }
    *out_nio = handle;
    return DYN_OK;
  } catch (const std::bad_alloc &) {
    if (out_nio != nullptr)
      *out_nio = nullptr;
    return DYN_ERR_OUT_OF_MEMORY;
  } catch (...) {
    if (out_nio != nullptr)
      *out_nio = nullptr;
    return DYN_ERR_INTERNAL;
  }
}

cfn dyn_nio_create_udp_auto(const char *name, const char *local_addr,
                            uint16_t port_start, uint16_t port_end,
                            dyn_nio **out_nio, uint16_t *out_local_port)
    -> dyn_result {
  try {
    if (out_nio != nullptr)
      *out_nio = nullptr;
    if (out_local_port != nullptr)
      *out_local_port = 0;
    if (out_nio == nullptr || out_local_port == nullptr)
      return DYN_ERR_INVALID_ARGUMENT;
    if (invalid_text(name) || local_addr == nullptr || port_start > port_end)
      return DYN_ERR_INVALID_ARGUMENT;
    if (const let status = require_runtime(); status != DYN_OK)
      return status;

    let *value =
        dyn_core_nio_create_udp_auto(name, local_addr, port_start, port_end);
    if (value == nullptr)
      return DYN_ERR_CREATE_FAILED;
    const let port = dyn_core_nio_udp_auto_local_port(value);
    if (port < 0 || port > UINT16_MAX) {
      dyn_core_nio_release(value);
      dyn_core_nio_delete(name);
      return DYN_ERR_INTERNAL;
    }
    let *handle = new (std::nothrow) dyn_nio{value};
    if (handle == nullptr) {
      dyn_core_nio_release(value);
      dyn_core_nio_delete(name);
      return DYN_ERR_OUT_OF_MEMORY;
    }
    *out_nio = handle;
    *out_local_port = static_cast<uint16_t>(port);
    return DYN_OK;
  } catch (const std::bad_alloc &) {
    return DYN_ERR_OUT_OF_MEMORY;
  } catch (...) {
    return DYN_ERR_INTERNAL;
  }
}

cfn dyn_nio_release(dyn_nio *nio) -> void {
  try {
    if (nio == nullptr)
      return;
    dyn_core_nio_release(nio->value);
    delete nio;
  } catch (...) {
  }
}

cfn dyn_nio_get_stats(const dyn_nio *nio, dyn_nio_stats *out_stats)
    -> dyn_result {
  try {
    if (out_stats == nullptr)
      return DYN_ERR_INVALID_ARGUMENT;
    *out_stats = {};
    if (nio == nullptr || nio->value == nullptr)
      return DYN_ERR_INVALID_ARGUMENT;
    if (const let status = require_runtime(); status != DYN_OK)
      return status;
    dyn_core_nio_get_stats(nio->value, out_stats);
    return DYN_OK;
  } catch (...) {
    return DYN_ERR_INTERNAL;
  }
}

cfn dyn_vm_attach_nio(dyn_vm *vm, uint32_t slot, uint32_t port, dyn_nio *nio)
    -> dyn_result {
  try {
    if (vm == nullptr || vm->value == nullptr || nio == nullptr ||
        nio->value == nullptr)
      return DYN_ERR_INVALID_ARGUMENT;
    if (const let status = require_runtime(); status != DYN_OK)
      return status;
    return dyn_core_vm_attach_nio(vm->value, slot, port, nio->value) == 0
               ? DYN_OK
               : DYN_ERR_BINDING_FAILED;
  } catch (...) {
    return DYN_ERR_INTERNAL;
  }
}

cfn dyn_eth_switch_create(const char *name, dyn_eth_switch **out_switch)
    -> dyn_result {
  try {
    if (out_switch == nullptr)
      return DYN_ERR_INVALID_ARGUMENT;
    *out_switch = nullptr;
    if (invalid_text(name))
      return DYN_ERR_INVALID_ARGUMENT;
    if (const let status = require_runtime(); status != DYN_OK)
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
  try {
    if (sw == nullptr)
      return;
    dyn_core_eth_switch_release(sw->value);
    delete sw;
  } catch (...) {
  }
}

cfn dyn_eth_switch_add_nio(dyn_eth_switch *sw, dyn_nio *nio) -> dyn_result {
  try {
    if (sw == nullptr || sw->value == nullptr || nio == nullptr ||
        nio->value == nullptr)
      return DYN_ERR_INVALID_ARGUMENT;
    if (const let status = require_runtime(); status != DYN_OK)
      return status;
    return dyn_core_eth_switch_add_nio(sw->value, nio->value) == 0
               ? DYN_OK
               : DYN_ERR_BINDING_FAILED;
  } catch (...) {
    return DYN_ERR_INTERNAL;
  }
}
