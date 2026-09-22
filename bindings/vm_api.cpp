#include "api_internal.h"

#include <new>

cfn dyn_vm_create(const char *name, int32_t instance_id, const char *platform,
                  dyn_vm **out_vm) -> dyn_result {
  try {
    if (out_vm == nullptr)
      return DYN_ERR_INVALID_ARGUMENT;
    *out_vm = nullptr;
    if (dyn_invalid_text(name) || dyn_invalid_text(platform))
      return DYN_ERR_INVALID_ARGUMENT;

    if (const let status = dyn_require_runtime(); status != DYN_OK)
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
    if (const let status = dyn_require_runtime(); status != DYN_OK)
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
    if (const let status = dyn_require_runtime(); status != DYN_OK)
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
    if (const let status = dyn_require_runtime(); status != DYN_OK)
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
    if (const let status = dyn_require_runtime(); status != DYN_OK)
      return status;
    dyn_core_vm_set_ram(vm->value, megabytes);
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
    if (const let status = dyn_require_runtime(); status != DYN_OK)
      return status;
    return dyn_core_vm_attach_nio(vm->value, slot, port, nio->value) == 0
               ? DYN_OK
               : DYN_ERR_BINDING_FAILED;
  } catch (...) {
    return DYN_ERR_INTERNAL;
  }
}

cfn dyn_vm_extract_config(dyn_vm *vm, dyn_bytes *out_startup,
                          dyn_bytes *out_private) -> dyn_result {
  try {
    if (out_startup != nullptr)
      *out_startup = {};
    if (out_private != nullptr)
      *out_private = {};
    if (out_startup == nullptr || out_private == nullptr ||
        out_startup == out_private || vm == nullptr || vm->value == nullptr)
      return DYN_ERR_INVALID_ARGUMENT;
    if (const let status = dyn_require_runtime(); status != DYN_OK)
      return status;
    const let result =
        dyn_core_vm_extract_config(vm->value, out_startup, out_private);
    if (result == 0)
      return DYN_OK;
    dyn_bytes_release(out_startup);
    dyn_bytes_release(out_private);
    return result == -2 ? DYN_ERR_UNSUPPORTED : DYN_ERR_IO;
  } catch (...) {
    dyn_bytes_release(out_startup);
    if (out_private != out_startup)
      dyn_bytes_release(out_private);
    return DYN_ERR_INTERNAL;
  }
}
