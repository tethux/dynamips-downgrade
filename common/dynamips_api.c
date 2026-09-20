#include <dynamips/dynamips.h>

const char *dyn_hello(void) { return ("hello from dynamips"); }

const char *dyn_result_message(dyn_result result) {
  switch (result) {
  case DYN_OK:
    return ("success");
  case DYN_ERR_INVALID_ARGUMENT:
    return ("invalid argument");
  case DYN_ERR_NOT_INITIALIZED:
    return ("runtime not initialized");
  case DYN_ERR_ALREADY_INITIALIZED:
    return ("runtime already initialized");
  case DYN_ERR_OUT_OF_MEMORY:
    return ("out of memory");
  case DYN_ERR_CREATE_FAILED:
    return ("create failed");
  case DYN_ERR_START_FAILED:
    return ("start failed");
  case DYN_ERR_STOP_FAILED:
    return ("stop failed");
  case DYN_ERR_INTERNAL:
    return ("internal error");
  }

  return ("unknown error");
}
