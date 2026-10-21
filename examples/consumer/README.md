# Standalone Go consumer

This separate module imports the published Dynamips package without a local
replacement. Install the matching native release and set `PKG_CONFIG_PATH`
as described in the [root README](../../README.md#go-consumers), then run:

```sh
cd examples/consumer
go mod tidy
CGO_ENABLED=1 go run .
```

The example pins `v0.2.28`. Its native libraries come from release assets,
not the Go module. For another consumer:

```sh
go get github.com/tethux/dynamips-downgrade@v0.2.28
```

The example creates native runtime files in a temporary directory and removes
them after shutdown. It leaves the checkout unchanged.
