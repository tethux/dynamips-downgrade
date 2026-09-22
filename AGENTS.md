# Scoped binding work

Tethux owns topology and inventory. The legacy hypervisor is behavioral
documentation, not the API specification. Bind only operations marked
`required` in `bindings/schema/`. `deferred` means potentially useful later;
`legacy_only` means the operation disappears with the text hypervisor.
Do not backfill adjacent commands or pursue hypervisor parity.

Read `bindings/README.md`, the relevant domain schema, each assigned `cmd_*`
handler, and every called function before implementing a binding. Preserve
direct state changes and side effects in the domain bridge. Never call a
`cmd_*` handler or pass hypervisor command strings through the new API.

Every required operation needs a typed C ABI, a C++23 module binding, a Go
binding when Tethux uses it, and a domain schema entry. Use public handles
for relationships. Return typed values, owned bytes or snapshots, and explicit
slices instead of CLI text or base64. Clear C outputs before work and release
native ownership explicitly. Keep C++ declarations in the relevant module
partition. C++ handles are move-only RAII objects; Go handles use explicit
Close. Calls remain externally serialized. Do not call Go from native workers.

Do not invent deletion, ownership, platform casts, callback, or console
semantics. Record unresolved semantics in the domain schema and resolve them
before calling the required scope complete. Verify new bindings through the
real integration where safely available. Run focused `mise` tasks;
`mise run compiledb` prepares clangd for C and C++.

Domain ownership keeps parallel work independent: a VM worker edits VM files,
a NIO worker edits NIO files, and a platform worker edits platform files.
Coordinate any common boundary change before editing shared files.
