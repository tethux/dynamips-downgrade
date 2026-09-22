#include "api_internal.h"

#include <climits>
#include <new>
#include <string>
#include <vector>

cfn dyn_nio_create_udp(const char *name, uint16_t local_port,
                       const char *remote_host, uint16_t remote_port,
                       dyn_nio **out_nio) -> dyn_result {
  try {
    if (out_nio == nullptr)
      return DYN_ERR_INVALID_ARGUMENT;
    *out_nio = nullptr;
    if (dyn_invalid_text(name) || dyn_invalid_text(remote_host))
      return DYN_ERR_INVALID_ARGUMENT;

    if (const let status = dyn_require_runtime(); status != DYN_OK)
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
    if (dyn_invalid_text(name) || local_addr == nullptr ||
        port_start > port_end)
      return DYN_ERR_INVALID_ARGUMENT;
    if (const let status = dyn_require_runtime(); status != DYN_OK)
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
    if (const let status = dyn_require_runtime(); status != DYN_OK)
      return status;
    dyn_core_nio_get_stats(nio->value, out_stats);
    return DYN_OK;
  } catch (...) {
    return DYN_ERR_INTERNAL;
  }
}

cfn dyn_nio_setup_filter(dyn_nio *nio, dyn_filter_direction direction,
                         size_t option_count, const char *const options[])
    -> dyn_result {
  try {
    if (nio == nullptr || nio->value == nullptr ||
        (direction != DYN_FILTER_RX && direction != DYN_FILTER_TX &&
         direction != DYN_FILTER_BOTH) ||
        option_count > INT_MAX || (option_count != 0 && options == nullptr))
      return DYN_ERR_INVALID_ARGUMENT;
    for (size_t i = 0; i < option_count; ++i)
      if (options[i] == nullptr)
        return DYN_ERR_INVALID_ARGUMENT;
    if (const let status = dyn_require_runtime(); status != DYN_OK)
      return status;

    std::vector<std::string> owned;
    owned.reserve(option_count);
    for (size_t i = 0; i < option_count; ++i)
      owned.emplace_back(options[i]);
    std::vector<char *> argv;
    argv.reserve(option_count);
    for (auto &option : owned)
      argv.push_back(option.data());
    return dyn_core_nio_setup_filter(nio->value, direction,
                                     static_cast<int>(option_count),
                                     argv.data()) == 0
               ? DYN_OK
               : DYN_ERR_INVALID_STATE;
  } catch (const std::bad_alloc &) {
    return DYN_ERR_OUT_OF_MEMORY;
  } catch (...) {
    return DYN_ERR_INTERNAL;
  }
}
