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

} // namespace dynamips
