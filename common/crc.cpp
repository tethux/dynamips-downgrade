#include "crc.h"

#include <cstdint>
#include <zlib.h>

#include <dynamips/macros.h>

cfn crc32_compute(std::uint32_t accumulator, std::uint8_t *data, int length)
    -> std::uint32_t {
  if (length <= 0)
    return ~accumulator;

  // zlib takes a completed CRC; legacy callers pass the raw accumulator.
  return static_cast<std::uint32_t>(
      crc32_z(~accumulator, data, static_cast<z_size_t>(length)));
}
