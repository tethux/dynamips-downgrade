#pragma once

#include <dynamips/macros.h>
#include <cstddef>
#include <cstdlib>
#include <expected>
#include <memory>
#include <system_error>

namespace dynamips::host {
struct free_bytes {
  fn operator()(unsigned char *ptr) const noexcept -> void { std::free(ptr); }
};
using owned_bytes = std::unique_ptr<unsigned char, free_bytes>;
struct file_buffer {
  owned_bytes bytes;
  std::size_t size;
};

fn read_file(const char *filename, bool read_contents = true) noexcept
    -> std::expected<file_buffer, std::error_code>;
fn allocate_aligned(std::size_t alignment, std::size_t size) noexcept
    -> std::expected<owned_bytes, std::error_code>;
} // namespace dynamips::host
