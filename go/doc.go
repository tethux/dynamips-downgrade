// Package dynamips provides typed Go bindings for the embedded Dynamips
// runtime. The runtime is process wide and can be initialized once. Calls are
// serialized around legacy global state. Close every VM, NIO, and switch
// handle before closing the runtime.
//
// The Go package links against native libraries. Build them with
// `mise run build:native`. See the [C7200 boot example] for a runnable router.
//
// [C7200 boot example]: https://github.com/tethux/dynamips-downgrade/tree/master/go/examples/basic
package dynamips
