module;

#include <dynamips/dynamips.h>
#include <dynamips/macros.h>

#include <cstdint>
#include <expected>
#include <new>
#include <string>
#include <string_view>

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

fn nio::stats() const noexcept -> result<nio_stats> {
  dyn_nio_stats value{};
  const let status = dyn_nio_get_stats(handle_, &value);
  if (status != DYN_OK)
    return std::unexpected(to_error(status));
  return nio_stats{value.packets_in, value.packets_out, value.bytes_in,
                   value.bytes_out};
}

} // namespace dynamips
