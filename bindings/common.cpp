#include "api_internal.h"

#include <cstdlib>

cfn dyn_bytes_release(dyn_bytes *bytes) -> void {
  if (bytes == nullptr)
    return;
  std::free(bytes->data);
  *bytes = {};
}
