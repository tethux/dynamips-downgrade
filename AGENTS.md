# Core migration

Tethux in `../sme` owns topology, inventory, provider orchestration, storage
leases, port allocation, and the external Ethernet fabric. Read the relevant
Tethux consumer before choosing a replacement. Do not recreate those features
inside Dynamips or pursue legacy hypervisor parity.

Dynamips owns emulation: native VM state and lifecycle, CPUs, guest memory,
cards, device behavior, native state-file formats, console endpoints, and NIO
endpoints attached to devices. Tethux orchestration does not make `stable/vm.c`
or native object lifetimes redundant. Tethux chooses storage directories;
Dynamips reads and writes the native files within them.

Keep existing public contracts. `bindings/README.md` and `bindings/schema/`
document ownership and API scope; they are references, not a new binding-work
assignment. Do not expand APIs or regenerate bindings as incidental porting
work. Preserve registry references, externally serialized native calls, and
worker teardown before freeing callback state. Do not call Go from native
workers or let C++ exceptions cross the C ABI.

Port a bounded mechanism after auditing its callers, ownership, locking, and
failure behavior. Follow surrounding C++ style and `include/dynamips/macros.h`.
Keep C-facing declarations usable from C; introduce a C++ module only when it
serves an actual consumer. Do not rewrite constant tables or retained C bridge
files merely to reduce the C file count. Device and JIT work does not require
porting every utility first.

Use focused `mise.toml` tasks. Verify the changed behavior with real inputs;
a build or IOS boot alone does not prove packet delivery or teardown safety.
When switching an implementation, check xmake source selection and packaged
native archives so Go tests exercise the new code. `mise run compiledb` prepares
clangd. Keep legacy sources unless their deletion is requested.

Keep detailed migration tables and progress in locally excluded planning files,
not this file. Describe changes, create new jj changes, or push only when asked.
