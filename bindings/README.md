# Bindings architecture

`schema/` defines the public operation surface by domain. Each operation is
`required`, `deferred`, or `legacy_only`. Required operations serve Tethux
orchestration. Deferred operations may be useful later. Legacy-only operations
exist for the remote text protocol and disappear with it. Tethux owns topology
and inventory, so discovery text, rename commands, and debug output are not
part of the embedding API. Implement only assigned required operations.

Each implemented operation records its C ABI, C++ API, ownership, error
mapping, threading contract, and the legacy code whose behavior it preserves.

The public boundary has four layers:

1. `include/dynamips/dynamips.h` includes the domain C ABI headers.
2. `modules/dynamips.cppm` exports the `dynamips` C++23 module with
   `std::expected` and move-only RAII handles. C++ consumers use
   `import dynamips;`; there is no public C++ header.
3. `common/dynamips_bridge_*.c` isolates the C++-unsafe legacy headers.
   `bindings/*_api.cpp` implements the C ABI. The C++ module has focused
   partitions for runtime, VM, NIO, and Ethernet switch. Bindings never call
   `cmd_*` hypervisor handlers.
4. The `go/` package is the embedding interface. It owns C
   strings, serializes access to legacy global state, and exposes explicit
   `Close` methods with Tethux-style categorized operation errors.

The legacy registry owns VM, NIO, and Ethernet switch objects. A public handle owns one registry
reference and releases it when destroyed. All handles must be destroyed before
the process-global runtime. Calls are externally serialized until the legacy
global state can be isolated.
