module;

#include <expected>

#include <dynamips/dynamips.h>

export module dynamips:runtime;
import :core;

export namespace dynamips {

class runtime final {
public:
  [[nodiscard]] static result<runtime> init(int argc, char *argv[]) noexcept;
  [[nodiscard]] static result<runtime> init_embedded() noexcept;

  runtime(const runtime &) = delete;
  runtime &operator=(const runtime &) = delete;

  runtime(runtime &&other) noexcept : active_(other.active_) {
    other.active_ = false;
  }

  runtime &operator=(runtime &&other) noexcept {
    if (this == &other)
      return *this;
    reset();
    active_ = other.active_;
    other.active_ = false;
    return *this;
  }

  ~runtime() { reset(); }

private:
  runtime() = default;

  void reset() noexcept {
    if (!active_)
      return;
    detail::runtime_released();
    active_ = false;
  }

  bool active_ = true;
};

} // namespace dynamips
