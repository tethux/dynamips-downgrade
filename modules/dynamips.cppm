module;

#include <cstdint>
#include <expected>
#include <string_view>

#include <dynamips/dynamips.h>

export module dynamips;

export namespace dynamips {

enum class error : int {
  invalid_argument = DYN_ERR_INVALID_ARGUMENT,
  not_initialized = DYN_ERR_NOT_INITIALIZED,
  already_initialized = DYN_ERR_ALREADY_INITIALIZED,
  out_of_memory = DYN_ERR_OUT_OF_MEMORY,
  create_failed = DYN_ERR_CREATE_FAILED,
  start_failed = DYN_ERR_START_FAILED,
  stop_failed = DYN_ERR_STOP_FAILED,
  internal = DYN_ERR_INTERNAL,
};

template <typename T> using result = std::expected<T, error>;

enum class vm_status : int {
  halted = DYN_VM_HALTED,
  shutdown = DYN_VM_SHUTDOWN,
  running = DYN_VM_RUNNING,
  suspended = DYN_VM_SUSPENDED,
};

[[nodiscard]] constexpr error to_error(dyn_result value) noexcept {
  return static_cast<error>(value);
}

[[nodiscard]] inline std::string_view message(error value) noexcept {
  return dyn_result_message(static_cast<dyn_result>(value));
}

class runtime final {
public:
  [[nodiscard]] static result<runtime> init(int argc, char *argv[]) noexcept {
    const auto status = dyn_runtime_init(argc, argv);
    if (status != DYN_OK)
      return std::unexpected(to_error(status));
    return runtime();
  }

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
    dyn_runtime_shutdown();
    active_ = false;
  }

  bool active_ = true;
};

class vm final {
public:
  [[nodiscard]] static result<vm> create(std::string_view name,
                                         std::int32_t instance_id,
                                         std::string_view platform) noexcept;

  vm(const vm &) = delete;
  vm &operator=(const vm &) = delete;

  vm(vm &&other) noexcept : handle_(other.release()) {}

  vm &operator=(vm &&other) noexcept {
    if (this == &other)
      return *this;
    reset();
    handle_ = other.release();
    return *this;
  }

  ~vm() { reset(); }

  [[nodiscard]] result<void> start() noexcept;
  [[nodiscard]] result<void> stop() noexcept;
  [[nodiscard]] result<vm_status> status() const noexcept;

private:
  explicit vm(dyn_vm *handle) noexcept : handle_(handle) {}

  [[nodiscard]] dyn_vm *release() noexcept {
    auto *handle = handle_;
    handle_ = nullptr;
    return handle;
  }

  void reset() noexcept {
    dyn_vm_release(handle_);
    handle_ = nullptr;
  }

  dyn_vm *handle_ = nullptr;
};

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
