module;

#include <array>
#include <cstdint>
#include <string_view>

#include <dynamips/dynamips.h>

export module dynamips:platform;
import :core;
import :vm;

export namespace dynamips {

class c7200 final {
public:
  [[nodiscard]] static result<void> set_npe(vm &router,
                                            std::string_view type) noexcept;
  [[nodiscard]] static result<void>
  set_midplane(vm &router, std::string_view type) noexcept;
  [[nodiscard]] static result<void>
  set_mac_addr(vm &router, std::array<std::uint8_t, 6> mac) noexcept;
};

} // namespace dynamips
