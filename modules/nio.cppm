module;

#include <cstdint>
#include <string_view>

#include <dynamips/dynamips.h>

export module dynamips:nio;
import :core;

export namespace dynamips {

class nio final {
public:
  [[nodiscard]] static result<nio>
  create_udp(std::string_view name, std::uint16_t local_port,
             std::string_view remote_host, std::uint16_t remote_port) noexcept;

  nio(const nio &) = delete;
  nio &operator=(const nio &) = delete;

  nio(nio &&other) noexcept : handle_(other.release()) {}

  nio &operator=(nio &&other) noexcept {
    if (this == &other)
      return *this;
    reset();
    handle_ = other.release();
    return *this;
  }

  ~nio() { reset(); }

private:
  explicit nio(dyn_nio *handle) noexcept : handle_(handle) {}

  [[nodiscard]] dyn_nio *release() noexcept {
    auto *handle = handle_;
    handle_ = nullptr;
    return handle;
  }

  void reset() noexcept {
    dyn_nio_release(handle_);
    handle_ = nullptr;
  }

  dyn_nio *handle_ = nullptr;
};

} // namespace dynamips
