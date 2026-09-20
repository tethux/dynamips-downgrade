#include <dynamips/dynamips.hpp>
#include <dynamips/macros.h>

#include <new>
#include <string>

#include "dynamips_bridge.h"

struct dyn_vm {
  dyn_core_vm *value;
};

struct dyn_nio {
  dyn_core_nio *value;
};

namespace {

[[nodiscard]] fn invalid_text(const char *value) noexcept -> bool {
  return value == nullptr || value[0] == '\0';
}

[[nodiscard]] fn require_runtime() noexcept -> dyn_result {
  return dyn_runtime_is_initialized() ? DYN_OK : DYN_ERR_NOT_INITIALIZED;
}

[[nodiscard]] fn as_result(dyn_result status) noexcept
    -> dynamips::result<void> {
  if (status == DYN_OK)
    return {};
  return std::unexpected(dynamips::to_error(status));
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

cfn dyn_nio_release(dyn_nio *nio) -> void {
  try {
    if (nio == nullptr)
      return;
    dyn_core_nio_release(nio->value);
    delete nio;
  } catch (...) {
  }
}

namespace dynamips {

fn vm::create(std::string_view name, std::int32_t instance_id,
              std::string_view platform) noexcept -> result<vm> {
  try {
    const std::string owned_name(name);
    const std::string owned_platform(platform);
    dyn_vm *handle = nullptr;
    const let status = dyn_vm_create(owned_name.c_str(), instance_id,
                                     owned_platform.c_str(), &handle);
    if (status != DYN_OK)
      return std::unexpected(to_error(status));
    return vm(handle);
  } catch (const std::bad_alloc &) {
    return std::unexpected(error::out_of_memory);
  } catch (...) {
    return std::unexpected(error::internal);
  }
}

fn vm::start() noexcept -> result<void> {
  return as_result(dyn_vm_start(handle_));
}

fn vm::stop() noexcept -> result<void> {
  return as_result(dyn_vm_stop(handle_));
}

fn nio::create_udp(std::string_view name, std::uint16_t local_port,
                   std::string_view remote_host,
                   std::uint16_t remote_port) noexcept -> result<nio> {
  try {
    const std::string owned_name(name);
    const std::string owned_remote_host(remote_host);
    dyn_nio *handle = nullptr;
    const let status =
        dyn_nio_create_udp(owned_name.c_str(), local_port,
                           owned_remote_host.c_str(), remote_port, &handle);
    if (status != DYN_OK)
      return std::unexpected(to_error(status));
    return nio(handle);
  } catch (const std::bad_alloc &) {
    return std::unexpected(error::out_of_memory);
  } catch (...) {
    return std::unexpected(error::internal);
  }
}

} // namespace dynamips
