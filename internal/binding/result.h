#ifndef DYNAMIPS_PUBLIC_RESULT_H
#define DYNAMIPS_PUBLIC_RESULT_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum dyn_result {
  DYN_OK = 0,
  DYN_ERR_INVALID_ARGUMENT = -1,
  DYN_ERR_NOT_INITIALIZED = -2,
  DYN_ERR_ALREADY_INITIALIZED = -3,
  DYN_ERR_OUT_OF_MEMORY = -4,
  DYN_ERR_CREATE_FAILED = -5,
  DYN_ERR_START_FAILED = -6,
  DYN_ERR_STOP_FAILED = -7,
  DYN_ERR_INTERNAL = -8,
  DYN_ERR_BINDING_FAILED = -9,
  DYN_ERR_UNSUPPORTED = -10,
  DYN_ERR_IO = -11,
  DYN_ERR_INVALID_STATE = -12,
} dyn_result;

typedef struct dyn_bytes {
  uint8_t *data;
  size_t size;
} dyn_bytes;

void dyn_bytes_release(dyn_bytes *bytes);

const char *dyn_result_message(dyn_result result);

#ifdef __cplusplus
}
#endif

#endif
