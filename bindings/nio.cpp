module;

#include <dynamips/dynamips.h>
#include <dynamips/macros.h>

#include <cstdint>
#include <expected>
#include <new>
#include <span>
#include <string>
#include <string_view>
#include <vector>

module dynamips;

namespace dynamips {

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

fn nio::create_udp_auto(std::string_view name, std::string_view local_addr,
                        std::uint16_t port_start,
                        std::uint16_t port_end) noexcept
    -> result<udp_auto_result> {
  try {
    const std::string owned_name(name);
    const std::string owned_local_addr(local_addr);
    dyn_nio *handle = nullptr;
    std::uint16_t local_port = 0;
    const let status =
        dyn_nio_create_udp_auto(owned_name.c_str(), owned_local_addr.c_str(),
                                port_start, port_end, &handle, &local_port);
    if (status != DYN_OK)
      return std::unexpected(to_error(status));
    return udp_auto_result{nio(handle), local_port};
  } catch (const std::bad_alloc &) {
    return std::unexpected(error::out_of_memory);
  } catch (...) {
    return std::unexpected(error::internal);
  }
}

fn nio::create_tap(std::string_view name, std::string_view device) noexcept
    -> result<nio> {
  try {
    if (name.find('\0') != std::string_view::npos ||
        device.find('\0') != std::string_view::npos)
      return std::unexpected(error::invalid_argument);
    const std::string owned_name(name);
    const std::string owned_device(device);
    dyn_nio *handle = nullptr;
    const let status =
        dyn_nio_create_tap(owned_name.c_str(), owned_device.c_str(), &handle);
    if (status != DYN_OK)
      return std::unexpected(to_error(status));
    return nio(handle);
  } catch (const std::bad_alloc &) {
    return std::unexpected(error::out_of_memory);
  } catch (...) {
    return std::unexpected(error::internal);
  }
}

fn nio::connect_udp_auto(std::string_view remote_host,
                         std::uint16_t remote_port) noexcept -> result<void> {
  try {
    if (remote_host.find('\0') != std::string_view::npos)
      return std::unexpected(error::invalid_argument);
    const std::string owned_host(remote_host);
    const let status =
        dyn_nio_connect_udp_auto(handle_, owned_host.c_str(), remote_port);
    if (status != DYN_OK)
      return std::unexpected(to_error(status));
    return {};
  } catch (const std::bad_alloc &) {
    return std::unexpected(error::out_of_memory);
  } catch (...) {
    return std::unexpected(error::internal);
  }
}

fn nio::remove() noexcept -> result<void> {
  const let status = dyn_nio_delete(&handle_);
  if (status != DYN_OK)
    return std::unexpected(to_error(status));
  detail::handle_released();
  return {};
}

fn nio::stats() const noexcept -> result<nio_stats> {
  dyn_nio_stats value{};
  const let status = dyn_nio_get_stats(handle_, &value);
  if (status != DYN_OK)
    return std::unexpected(to_error(status));
  return nio_stats{value.packets_in, value.packets_out, value.bytes_in,
                   value.bytes_out};
}

fn nio::setup_filter(filter_direction direction,
                     std::span<const std::string_view> options) noexcept
    -> result<void> {
  try {
    std::vector<std::string> owned;
    owned.reserve(options.size());
    for (const auto option : options) {
      if (option.find('\0') != std::string_view::npos)
        return std::unexpected(error::invalid_argument);
      owned.emplace_back(option);
    }
    std::vector<const char *> argv;
    argv.reserve(owned.size());
    for (const auto &option : owned)
      argv.push_back(option.c_str());
    const let status = dyn_nio_setup_filter(
        handle_, static_cast<std::int32_t>(direction), argv.size(),
        argv.data());
    if (status != DYN_OK)
      return std::unexpected(to_error(status));
    return {};
  } catch (const std::bad_alloc &) {
    return std::unexpected(error::out_of_memory);
  } catch (...) {
    return std::unexpected(error::internal);
  }
}

} // namespace dynamips
