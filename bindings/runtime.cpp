module;

#include <dynamips/dynamips.h>
#include <dynamips/macros.h>

#include <expected>

module dynamips;

namespace dynamips {

fn runtime::init(int argc, char *argv[]) noexcept -> result<runtime> {
  const let status = dyn_runtime_init(argc, argv);
  if (status != DYN_OK)
    return std::unexpected(to_error(status));
  return runtime();
}

fn runtime::init_embedded() noexcept -> result<runtime> {
  const let status = dyn_runtime_init_embedded();
  if (status != DYN_OK)
    return std::unexpected(to_error(status));
  return runtime();
}

} // namespace dynamips
