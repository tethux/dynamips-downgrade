module;

#include <cstdint>
#include <cstddef>
#include <string_view>
#include <vector>

#include <dynamips/dynamips.h>

export module dynamips:vm;
import :core;
import :nio;

export namespace dynamips {

struct vm_config_data {
  std::vector<std::byte> startup;
  std::vector<std::byte> private_config;
};

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
  [[nodiscard]] result<void> delete_instance() noexcept;
  [[nodiscard]] result<void> suspend() noexcept;
  [[nodiscard]] result<void> resume() noexcept;
  [[nodiscard]] result<void> set_ios(std::string_view path) noexcept;
  [[nodiscard]] result<void> set_nvram(std::uint32_t kilobytes) noexcept;
  [[nodiscard]] result<void> set_sparse_mem(bool enabled) noexcept;
  [[nodiscard]] result<void> set_conf_reg(std::uint32_t value) noexcept;
  [[nodiscard]] result<void> set_idle_pc(std::uint64_t value) noexcept;
  [[nodiscard]] result<void> set_con_tcp_port(std::uint16_t port) noexcept;
  [[nodiscard]] result<void>
  push_config(const std::vector<std::byte> *startup,
              const std::vector<std::byte> *private_config) noexcept;
  [[nodiscard]] result<vm_status> status() const noexcept;
  [[nodiscard]] result<void> set_ram(std::uint32_t megabytes) noexcept;
  [[nodiscard]] result<void> add_card(std::uint32_t slot,
                                      std::string_view card) noexcept;
  [[nodiscard]] result<void> remove_card(std::uint32_t slot) noexcept;
  [[nodiscard]] result<void> attach_nio(std::uint32_t slot, std::uint32_t port,
                                        nio &endpoint) noexcept;
  [[nodiscard]] result<void> detach_nio(std::uint32_t slot,
                                        std::uint32_t port) noexcept;
  [[nodiscard]] result<vm_config_data> extract_config() noexcept;

private:
  friend class c7200;
  explicit vm(dyn_vm *handle) noexcept : handle_(handle) {
    detail::handle_acquired();
  }

  [[nodiscard]] dyn_vm *release() noexcept {
    auto *handle = handle_;
    handle_ = nullptr;
    return handle;
  }

  void reset() noexcept {
    if (handle_ == nullptr)
      return;
    dyn_vm_release(handle_);
    handle_ = nullptr;
    detail::handle_released();
  }

  dyn_vm *handle_ = nullptr;
};

} // namespace dynamips
