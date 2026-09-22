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
  if (vm == nullptr)
    return;
  dyn_core_vm_release(vm->value);
  delete vm;
}

cfn dyn_vm_delete(dyn_vm **vm) -> dyn_result {
  try {
    if (vm == nullptr || *vm == nullptr || (*vm)->value == nullptr)
      return DYN_ERR_INVALID_ARGUMENT;
    if (const let status = dyn_require_runtime(); status != DYN_OK)
      return status;
    const let result = dyn_core_vm_delete_handle(&(*vm)->value);
    if (result != 0)
      return result == -2 ? DYN_ERR_OUT_OF_MEMORY : DYN_ERR_INVALID_STATE;
    delete *vm;
    *vm = nullptr;
    return DYN_OK;
  } catch (...) {
    return DYN_ERR_INTERNAL;
  }
}

cfn dyn_vm_suspend(dyn_vm *vm) -> dyn_result {
  try {
    if (vm == nullptr || vm->value == nullptr)
      return DYN_ERR_INVALID_ARGUMENT;
    if (const let status = dyn_require_runtime(); status != DYN_OK)
      return status;
    return dyn_core_vm_suspend(vm->value) == 0 ? DYN_OK : DYN_ERR_INVALID_STATE;
  } catch (...) {
    return DYN_ERR_INTERNAL;
  }
}

cfn dyn_vm_resume(dyn_vm *vm) -> dyn_result {
  try {
    if (vm == nullptr || vm->value == nullptr)
      return DYN_ERR_INVALID_ARGUMENT;
    if (const let status = dyn_require_runtime(); status != DYN_OK)
      return status;
    return dyn_core_vm_resume(vm->value) == 0 ? DYN_OK : DYN_ERR_INVALID_STATE;
  } catch (...) {
    return DYN_ERR_INTERNAL;
  }
}

cfn dyn_vm_set_ios(dyn_vm *vm, const char *path) -> dyn_result {
  try {
    if (vm == nullptr || vm->value == nullptr || dyn_invalid_text(path))
      return DYN_ERR_INVALID_ARGUMENT;
    if (const let status = dyn_require_runtime(); status != DYN_OK)
      return status;
    return dyn_core_vm_set_ios(vm->value, path) == 0 ? DYN_OK
                                                     : DYN_ERR_OUT_OF_MEMORY;
  } catch (...) {
    return DYN_ERR_INTERNAL;
  }
}

cfn dyn_vm_set_nvram(dyn_vm *vm, uint32_t kilobytes) -> dyn_result {
  if (vm == nullptr || vm->value == nullptr)
    return DYN_ERR_INVALID_ARGUMENT;
  if (const let status = dyn_require_runtime(); status != DYN_OK)
    return status;
  dyn_core_vm_set_nvram(vm->value, kilobytes);
  return DYN_OK;
}

cfn dyn_vm_set_sparse_mem(dyn_vm *vm, int enabled) -> dyn_result {
  if (vm == nullptr || vm->value == nullptr || (enabled != 0 && enabled != 1))
    return DYN_ERR_INVALID_ARGUMENT;
  if (const let status = dyn_require_runtime(); status != DYN_OK)
    return status;
  dyn_core_vm_set_sparse_mem(vm->value, enabled);
  return DYN_OK;
}

cfn dyn_vm_set_conf_reg(dyn_vm *vm, uint32_t value) -> dyn_result {
  if (vm == nullptr || vm->value == nullptr)
    return DYN_ERR_INVALID_ARGUMENT;
  if (const let status = dyn_require_runtime(); status != DYN_OK)
    return status;
  dyn_core_vm_set_conf_reg(vm->value, value);
  return DYN_OK;
}

cfn dyn_vm_set_idle_pc(dyn_vm *vm, uint64_t value) -> dyn_result {
  if (vm == nullptr || vm->value == nullptr)
    return DYN_ERR_INVALID_ARGUMENT;
  if (const let status = dyn_require_runtime(); status != DYN_OK)
    return status;
  dyn_core_vm_set_idle_pc(vm->value, value);
  return DYN_OK;
}

cfn dyn_vm_set_con_tcp_port(dyn_vm *vm, uint16_t port) -> dyn_result {
  if (vm == nullptr || vm->value == nullptr || port == 0)
    return DYN_ERR_INVALID_ARGUMENT;
  if (const let status = dyn_require_runtime(); status != DYN_OK)
    return status;
  dyn_core_vm_set_con_tcp_port(vm->value, port);
  return DYN_OK;
}

cfn dyn_vm_push_config(dyn_vm *vm, const uint8_t *startup, size_t startup_size,
                       const uint8_t *private_config, size_t private_size)
    -> dyn_result {
  try {
    if (vm == nullptr || vm->value == nullptr ||
        (startup == nullptr && startup_size != 0) ||
        (private_config == nullptr && private_size != 0))
      return DYN_ERR_INVALID_ARGUMENT;
    if (const let status = dyn_require_runtime(); status != DYN_OK)
      return status;
    const let result = dyn_core_vm_push_config(vm->value, startup, startup_size,
                                               private_config, private_size);
    return result == 0    ? DYN_OK
           : result == -2 ? DYN_ERR_UNSUPPORTED
                          : DYN_ERR_IO;
  } catch (...) {
    return DYN_ERR_INTERNAL;
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

cfn dyn_vm_add_card(dyn_vm *vm, uint32_t slot, const char *card) -> dyn_result {
  try {
    if (vm == nullptr || vm->value == nullptr || dyn_invalid_text(card))
      return DYN_ERR_INVALID_ARGUMENT;
    if (const let status = dyn_require_runtime(); status != DYN_OK)
      return status;
    return dyn_core_vm_add_card(vm->value, slot, card) == 0
               ? DYN_OK
               : DYN_ERR_BINDING_FAILED;
  } catch (...) {
    return DYN_ERR_INTERNAL;
  }
}

cfn dyn_vm_remove_card(dyn_vm *vm, uint32_t slot) -> dyn_result {
  try {
    if (vm == nullptr || vm->value == nullptr)
      return DYN_ERR_INVALID_ARGUMENT;
    if (const let status = dyn_require_runtime(); status != DYN_OK)
      return status;
    return dyn_core_vm_remove_card(vm->value, slot) == 0
               ? DYN_OK
               : DYN_ERR_BINDING_FAILED;
  } catch (...) {
    return DYN_ERR_INTERNAL;
  }
}

cfn dyn_vm_detach_nio(dyn_vm *vm, uint32_t slot, uint32_t port) -> dyn_result {
  try {
    if (vm == nullptr || vm->value == nullptr)
      return DYN_ERR_INVALID_ARGUMENT;
    if (const let status = dyn_require_runtime(); status != DYN_OK)
      return status;
    return dyn_core_vm_detach_nio(vm->value, slot, port) == 0
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
