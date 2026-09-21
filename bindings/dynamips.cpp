module;

#include <dynamips/dynamips.h>
#include <dynamips/macros.h>

#include <cstdint>
#include <expected>
#include <new>
#include <string>
#include <string_view>

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

fn vm::status() const noexcept -> result<vm_status> {
  dyn_vm_status value = DYN_VM_HALTED;
  const let status = dyn_vm_get_status(handle_, &value);
  if (status != DYN_OK)
    return std::unexpected(to_error(status));
  return static_cast<vm_status>(value);
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
