#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Accept a raw accumulator and return the complemented CRC-32. */
uint32_t crc32_compute(uint32_t accumulator, uint8_t *data, int length);

#ifdef __cplusplus
}
#endif
