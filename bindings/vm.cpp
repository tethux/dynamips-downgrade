module;

#include <dynamips/dynamips.h>
#include <dynamips/macros.h>

#include <cstdint>
#include <cstring>
#include <expected>
#include <new>
#include <string>
#include <string_view>
#include <vector>

module dynamips;

namespace {

[[nodiscard]] fn as_result(dyn_result status) noexcept
    -> dynamips::result<void> {
  if (status == DYN_OK)
    return {};
  return std::unexpected(dynamips::to_error(status));
}

} // namespace

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

fn vm::delete_instance() noexcept -> result<void> {
  const let status = dyn_vm_delete(&handle_);
  if (status == DYN_OK)
    detail::handle_released();
  return as_result(status);
}

fn vm::suspend() noexcept -> result<void> {
  return as_result(dyn_vm_suspend(handle_));
}

fn vm::resume() noexcept -> result<void> {
  return as_result(dyn_vm_resume(handle_));
}

fn vm::set_ios(std::string_view path) noexcept -> result<void> {
  try {
    const std::string owned(path);
    return as_result(dyn_vm_set_ios(handle_, owned.c_str()));
  } catch (const std::bad_alloc &) {
    return std::unexpected(error::out_of_memory);
  } catch (...) {
    return std::unexpected(error::internal);
  }
}

fn vm::set_nvram(std::uint32_t kilobytes) noexcept -> result<void> {
  return as_result(dyn_vm_set_nvram(handle_, kilobytes));
}

fn vm::set_sparse_mem(bool enabled) noexcept -> result<void> {
  return as_result(dyn_vm_set_sparse_mem(handle_, enabled ? 1 : 0));
}

fn vm::set_conf_reg(std::uint32_t value) noexcept -> result<void> {
  return as_result(dyn_vm_set_conf_reg(handle_, value));
}

fn vm::set_idle_pc(std::uint64_t value) noexcept -> result<void> {
  return as_result(dyn_vm_set_idle_pc(handle_, value));
}

fn vm::set_con_tcp_port(std::uint16_t port) noexcept -> result<void> {
  return as_result(dyn_vm_set_con_tcp_port(handle_, port));
}

fn vm::push_config(const std::vector<std::byte> *startup,
                   const std::vector<std::byte> *private_config) noexcept
    -> result<void> {
  const auto *startup_data =
      startup == nullptr
          ? nullptr
          : reinterpret_cast<const std::uint8_t *>(startup->data());
  const auto *private_data =
      private_config == nullptr
          ? nullptr
          : reinterpret_cast<const std::uint8_t *>(private_config->data());
  return as_result(dyn_vm_push_config(
      handle_, startup_data, startup == nullptr ? 0 : startup->size(),
      private_data, private_config == nullptr ? 0 : private_config->size()));
}

fn vm::status() const noexcept -> result<vm_status> {
  dyn_vm_status value = DYN_VM_HALTED;
  const let status = dyn_vm_get_status(handle_, &value);
  if (status != DYN_OK)
    return std::unexpected(to_error(status));
  return static_cast<vm_status>(value);
}

fn vm::set_ram(std::uint32_t megabytes) noexcept -> result<void> {
  return as_result(dyn_vm_set_ram(handle_, megabytes));
}

fn vm::attach_nio(std::uint32_t slot, std::uint32_t port,
                  nio &endpoint) noexcept -> result<void> {
  return as_result(dyn_vm_attach_nio(handle_, slot, port, endpoint.handle_));
}

fn vm::add_card(std::uint32_t slot, std::string_view card) noexcept
    -> result<void> {
  try {
    const std::string owned(card);
    return as_result(dyn_vm_add_card(handle_, slot, owned.c_str()));
  } catch (const std::bad_alloc &) {
    return std::unexpected(error::out_of_memory);
  } catch (...) {
    return std::unexpected(error::internal);
  }
}

fn vm::remove_card(std::uint32_t slot) noexcept -> result<void> {
  return as_result(dyn_vm_remove_card(handle_, slot));
}

fn vm::detach_nio(std::uint32_t slot, std::uint32_t port) noexcept
    -> result<void> {
  return as_result(dyn_vm_detach_nio(handle_, slot, port));
}

fn vm::extract_config() noexcept -> result<vm_config_data> {
  dyn_bytes startup{};
  dyn_bytes private_config{};
  const let status = dyn_vm_extract_config(handle_, &startup, &private_config);
  if (status != DYN_OK)
    return std::unexpected(to_error(status));
  try {
    vm_config_data result;
    result.startup.resize(startup.size);
    result.private_config.resize(private_config.size);
    if (startup.size != 0)
      std::memcpy(result.startup.data(), startup.data, startup.size);
    if (private_config.size != 0)
      std::memcpy(result.private_config.data(), private_config.data,
                  private_config.size);
    dyn_bytes_release(&startup);
    dyn_bytes_release(&private_config);
    return result;
  } catch (const std::bad_alloc &) {
    dyn_bytes_release(&startup);
    dyn_bytes_release(&private_config);
    return std::unexpected(error::out_of_memory);
  } catch (...) {
    dyn_bytes_release(&startup);
    dyn_bytes_release(&private_config);
    return std::unexpected(error::internal);
  }
}

} // namespace dynamips
