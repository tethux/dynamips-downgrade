module;

#include <cstdint>
#include <string_view>
#include <span>

#include <dynamips/dynamips.h>

export module dynamips:nio;
import :core;

export namespace dynamips {

struct nio_stats {
  std::uint64_t packets_in;
  std::uint64_t packets_out;
  std::uint64_t bytes_in;
  std::uint64_t bytes_out;
};

struct udp_auto_result;

enum class filter_direction : int {
  rx = DYN_FILTER_RX,
  tx = DYN_FILTER_TX,
  both = DYN_FILTER_BOTH,
};

class nio final {
public:
  [[nodiscard]] static result<nio>
  create_udp(std::string_view name, std::uint16_t local_port,
             std::string_view remote_host, std::uint16_t remote_port) noexcept;
  [[nodiscard]] static result<udp_auto_result>
  create_udp_auto(std::string_view name, std::string_view local_addr,
                  std::uint16_t port_start, std::uint16_t port_end) noexcept;

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

  [[nodiscard]] result<nio_stats> stats() const noexcept;
  [[nodiscard]] result<void>
  setup_filter(filter_direction direction,
               std::span<const std::string_view> options) noexcept;

private:
  friend class vm;
  friend class ethernet_switch;
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

struct udp_auto_result {
  nio endpoint;
  std::uint16_t local_port;
};

} // namespace dynamips
