module;

#include <string_view>

#include <dynamips/dynamips.h>

export module dynamips:eth_switch;
import :core;
import :nio;

export namespace dynamips {

class ethernet_switch final {
public:
  [[nodiscard]] static result<ethernet_switch>
  create(std::string_view name) noexcept;

  ethernet_switch(const ethernet_switch &) = delete;
  ethernet_switch &operator=(const ethernet_switch &) = delete;

  ethernet_switch(ethernet_switch &&other) noexcept
      : handle_(other.release()) {}

  ethernet_switch &operator=(ethernet_switch &&other) noexcept {
    if (this == &other)
      return *this;
    reset();
    handle_ = other.release();
    return *this;
  }

  ~ethernet_switch() { reset(); }

  [[nodiscard]] result<void> add(nio &endpoint) noexcept;

private:
  explicit ethernet_switch(dyn_eth_switch *handle) noexcept : handle_(handle) {}

  [[nodiscard]] dyn_eth_switch *release() noexcept {
    auto *handle = handle_;
    handle_ = nullptr;
    return handle;
  }

  void reset() noexcept {
    dyn_eth_switch_release(handle_);
    handle_ = nullptr;
  }

  dyn_eth_switch *handle_ = nullptr;
};

} // namespace dynamips
