module;

#include <dynamips/dynamips.h>
#include <dynamips/macros.h>

#include <expected>

module dynamips;

namespace dynamips {

namespace detail {

namespace {
unsigned live_handles = 0;
bool owns_runtime = false;
bool runtime_released_by_owner = false;
} // namespace

void runtime_acquired() noexcept {
  owns_runtime = true;
  runtime_released_by_owner = false;
}

void runtime_released() noexcept {
  runtime_released_by_owner = true;
  if (live_handles == 0) {
    dyn_runtime_shutdown();
    owns_runtime = false;
  }
}

void handle_acquired() noexcept { ++live_handles; }

void handle_released() noexcept {
  --live_handles;
  if (live_handles == 0 && owns_runtime && runtime_released_by_owner) {
    dyn_runtime_shutdown();
    owns_runtime = false;
  }
}

} // namespace detail

fn runtime::init(int argc, char *argv[]) noexcept -> result<runtime> {
  const let status = dyn_runtime_init(argc, argv);
  if (status != DYN_OK)
    return std::unexpected(to_error(status));
  detail::runtime_acquired();
  return runtime();
}

fn runtime::init_embedded() noexcept -> result<runtime> {
  const let status = dyn_runtime_init_embedded();
  if (status != DYN_OK)
    return std::unexpected(to_error(status));
  detail::runtime_acquired();
  return runtime();
}

} // namespace dynamips
