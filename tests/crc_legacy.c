#include <stdint.h>

extern uint32_t crc32_array[];

uint32_t crc_test_legacy(uint32_t accumulator, uint8_t *data, int length) {
  for (int index = 0; index < length; ++index)
    accumulator =
        crc32_array[(accumulator ^ data[index]) & 0xff] ^ (accumulator >> 8);
  return ~accumulator;
}
