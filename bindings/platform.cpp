module;

#include <dynamips/dynamips.h>
#include <dynamips/macros.h>

#include <array>
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

fn c7200::set_npe(vm &router, std::string_view type) noexcept -> result<void> {
  try {
    const std::string owned(type);
    return as_result(dyn_c7200_set_npe(router.handle_, owned.c_str()));
  } catch (const std::bad_alloc &) {
    return std::unexpected(error::out_of_memory);
  } catch (...) {
    return std::unexpected(error::internal);
  }
}

fn c7200::set_midplane(vm &router, std::string_view type) noexcept
    -> result<void> {
  try {
    const std::string owned(type);
    return as_result(dyn_c7200_set_midplane(router.handle_, owned.c_str()));
  } catch (const std::bad_alloc &) {
    return std::unexpected(error::out_of_memory);
  } catch (...) {
    return std::unexpected(error::internal);
  }
}

fn c7200::set_mac_addr(vm &router, std::array<std::uint8_t, 6> mac) noexcept
    -> result<void> {
  return as_result(dyn_c7200_set_mac_addr(router.handle_, mac.data()));
}

} // namespace dynamips
