# Bindings architecture

`schema.yaml` is the source of truth for the public operation surface. Each
operation records its C ABI, C++ API, ownership, error mapping, threading
contract, and the legacy code whose behavior it preserves.

The public boundary has four layers:

1. `dynamips.h` is the stable C ABI intended for Go and other FFI consumers.
2. `dynamips.hpp` provides small C++23 wrappers using `std::expected` and
   move-only RAII handles.
3. `dynamips_bridge.c` isolates the C++-unsafe legacy headers.
   `dynamips.cpp` follows the same C ABI boundary style and `macros.h`
   vocabulary as shitnet, then maps that ABI into C++ values. Future generated
   modules should follow this split and never call `cmd_*` hypervisor handlers.
4. The root Go package is the normal test and embedding interface. It owns C
   strings, serializes access to legacy global state, and exposes explicit
   `Close` methods with Tethux-style categorized operation errors.

The legacy registry owns VM and NIO objects. A public handle owns one registry
reference and releases it when destroyed. All handles must be destroyed before
the process-global runtime. Calls are externally serialized until the legacy
global state can be isolated.
