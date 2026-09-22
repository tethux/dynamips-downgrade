module;

#include <cstdint>
#include <string_view>

#include <dynamips/dynamips.h>

export module dynamips:vm;
import :core;

export namespace dynamips {

enum class vm_status : int {
  halted = DYN_VM_HALTED,
  shutdown = DYN_VM_SHUTDOWN,
  running = DYN_VM_RUNNING,
  suspended = DYN_VM_SUSPENDED,
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

} // namespace dynamips
