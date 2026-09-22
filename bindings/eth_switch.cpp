module;

#include <dynamips/dynamips.h>
#include <dynamips/macros.h>

#include <expected>
#include <new>
#include <string>
#include <string_view>

module dynamips;

namespace dynamips {

fn ethernet_switch::create(std::string_view name) noexcept
    -> result<ethernet_switch> {
  try {
    const std::string owned_name(name);
    dyn_eth_switch *handle = nullptr;
    const let status = dyn_eth_switch_create(owned_name.c_str(), &handle);
    if (status != DYN_OK)
      return std::unexpected(to_error(status));
    return ethernet_switch(handle);
  } catch (const std::bad_alloc &) {
    return std::unexpected(error::out_of_memory);
  } catch (...) {
    return std::unexpected(error::internal);
  }
}

fn ethernet_switch::add(nio &endpoint) noexcept -> result<void> {
  const let status = dyn_eth_switch_add_nio(handle_, endpoint.handle_);
  if (status != DYN_OK)
    return std::unexpected(to_error(status));
  return {};
}

} // namespace dynamips
