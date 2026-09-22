module;

#include <expected>
#include <string_view>

#include <dynamips/dynamips.h>

export module dynamips:core;

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
  binding_failed = DYN_ERR_BINDING_FAILED,
  unsupported = DYN_ERR_UNSUPPORTED,
  io = DYN_ERR_IO,
};

template <typename T> using result = std::expected<T, error>;

[[nodiscard]] constexpr error to_error(dyn_result value) noexcept {
  return static_cast<error>(value);
}

[[nodiscard]] inline std::string_view message(error value) noexcept {
  return dyn_result_message(static_cast<dyn_result>(value));
}

} // namespace dynamips
