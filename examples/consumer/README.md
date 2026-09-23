# Standalone Go consumer

This directory has its own Go module. It imports the published
`github.com/tethux/dynamips-downgrade` package, initializes Dynamips, and
closes it.

On Linux amd64, install a C++ compiler and the libelf, libpcap, and libnsl
development libraries. Then run:

```sh
cd examples/consumer
CGO_ENABLED=1 go run .
```

The example's `go.mod` pins `v0.2.27`. In your own module, add it with
`go get github.com/tethux/dynamips-downgrade@v0.2.27`.
