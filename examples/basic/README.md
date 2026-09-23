# C7200 boot example

Go reference: [examples/basic](https://pkg.go.dev/github.com/tethux/dynamips-downgrade/examples/basic).

Set the path to a local C7200 IOS image or zip archive:

```sh
DYNAMIPS_IOS_IMAGE=/path/to/c7200-ios.zip mise run run:example
```

The program also accepts RAM and console options directly:

```sh
mise run build:native
PKG_CONFIG_PATH="$PWD/build/install/lib/pkgconfig" mise exec -- go run ./examples/basic -ios /path/to/c7200-ios.bin -ram 512 -console-port 2001
```

The task opens `telnet` in the same terminal once the VM starts. Wait for
`Press RETURN to get started!`, then press Enter. The example loads a small
startup config, so IOS skips the initial setup dialog. Type IOS commands at
the prompt. For example, enter `enable`, then `show version`. Press Ctrl+]
and type `quit` to stop and delete the VM.

The example creates temporary working files outside the repository and
removes them when it exits. Cisco IOS images are not included here.
