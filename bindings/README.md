# Bindings architecture

`schema.yaml` is the source of truth for the public operation surface. Each
operation records its C ABI, C++ API, ownership, error mapping, threading
contract, and the legacy code whose behavior it preserves.

The public boundary has four layers:

1. `dynamips.h` is the stable C ABI intended for Go and other FFI consumers.
2. `modules/dynamips.cppm` exports the `dynamips` C++23 module with
   `std::expected` and move-only RAII handles. C++ consumers use
   `import dynamips;`; there is no public C++ header.
3. `dynamips_bridge.c` isolates the C++-unsafe legacy headers. `api.cpp`
   implements the C ABI. The C++ interface has focused module partitions for
   core types, runtime, VM, and NIO; `vm.cpp` and `nio.cpp` implement their
   operations. The bindings use the same boundary style and `macros.h`
   vocabulary as shitnet and never call `cmd_*` hypervisor handlers.
4. The root Go package is the normal test and embedding interface. It owns C
   strings, serializes access to legacy global state, and exposes explicit
   `Close` methods with Tethux-style categorized operation errors.

The legacy registry owns VM and NIO objects. A public handle owns one registry
reference and releases it when destroyed. All handles must be destroyed before
the process-global runtime. Calls are externally serialized until the legacy
global state can be isolated.
