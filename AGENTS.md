# Binding backfill

Every legacy hypervisor operation needs a typed C ABI function, a
C++23 module binding, a Go binding, and an entry in `bindings/schema.yaml`.
Read the `cmd_*` handler and the functions it calls before implementing one;
preserve direct state changes and side effects in `common/dynamips_bridge.c`.
Use the current Status, SetRAM, Stats, UDPAuto, AttachNIO, EthernetSwitch,
ExtractConfig, and SetupFilter bindings as examples for each API shape.

Use handles for relationships between public objects. Return typed values,
owned bytes or snapshots, and explicit slices instead of CLI text or base64.
Initialize C outputs before work and release native ownership explicitly.
Keep C++ declarations in the relevant module partition, not the primary module.

Do not guess deletion, rename, console/event, or platform-cast semantics.
Record any operation that cannot yet be bound in the schema with the reason,
then resolve it explicitly before calling the backfill complete. Verify each
new binding through the real integration where safely available. Run the
focused `mise` tasks; `mise run compiledb` prepares clangd for C and C++.
