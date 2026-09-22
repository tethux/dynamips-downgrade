# C7200 boot example

Build the native library, then pass an extracted local C7200 IOS image:

```sh
mise run build:native
mise exec -- go run ./go/examples/basic -ios /path/to/c7200-ios.bin -ram 256 -console-port 2000
```

Connect to `127.0.0.1:2000` with a raw TCP client to use the IOS console.
Press Ctrl-C to stop and delete the VM. The example creates temporary working
files outside the repository and removes them when it exits. Cisco IOS images
are not included here.
