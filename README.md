# Dynamips (Cisco Router Emulator)

## Overview

Authors of this document: Fabien Devaux, Christophe Fillot, MtvE, 
Gordon Russell, Jeremy Grossmann and Flávio J. Saraiva.

Converted to markdown format by Daniel Lintott.

This is a continuation of Dynamips, based on the last development version and 
improved with patches wrote by various people from the community. This fork was
named Dynamips-community up to the 0.2.8-community release and renamed to the 
original Dynamips on the 0.2.9 release.

This fork builds the stable emulator core. The old text hypervisor remains as
a separate compatibility target for behavior comparison while Tethux uses the
typed embedding API. The required operation scope and deferred legacy commands
are recorded in `bindings/schema/`.

This is an experimental, AI-assisted learning project. AI agents helped with
the scoped binding backfill, refactor, and tests. I am using it to learn the
emulator and improve my development workflow. I do not claim this work is
better than the years of engineering behind Dynamips. I am having fun, and my
long-term goal is to rewrite most of the code into a smaller emulator that
contains only what I need.

License: GNU GPLv2 only

### Direction of this fork

This fork is not an attempt to pretend the old Dynamips internals are modern.
The code is useful and proven, but it also has decades of global state, coupled
runtime and CLI behavior, old JIT assumptions, and interfaces that are hard to
embed safely. It is kind of bad by current standards, and the point of this
work is to put a small, honest boundary around it before changing more of it.

The current C ABI, C++ module, and Go package cover the scoped C7200
orchestration operations in `bindings/schema/`. The old hypervisor remains a
behavioral reference for integration tests. My longer-term plan is to replace
legacy internals behind this boundary as the smaller emulator becomes useful.

The original document authors, maintainers, and community contributors remain
credited here, in `MAINTAINERS`, and throughout `ChangeLog`. Special thanks to
Christophe Fillot for creating Dynamips and to GNS3 and its contributors for
maintaining and publishing the code under GPLv2. That is what makes this fork,
and the frankly shameless borrowing of a working emulator, possible.

### How to compile Dynamips

Dynamips uses xmake with Clang, C23, and C++23. The embedding API also has Go
bindings. mise provides the normal project commands and
tool versions. xmake uses installed dependencies first and falls back to its
package repository when necessary.

#### Build Dependencies

On Debian based systems the following build dependencies are required and can be
installed using apt-get:

- libelf-dev
- libpcap0.8-dev

On Redhat based systems (CentOS, Fedora etc) the following build dependencies are
required and can be installed using yum:

- elfutils-libelf-devel
- libpcap-devel

Similar packages should be available for most distributions, consult your
distributions package list to find them.

MacPort & Homebrew:

- libelf

#### Compiling (Linux/Mac)

Either download and extract a source tarball from the releases page or clone the
Git repository using:

```
git clone https://github.com/tethux/dynamips-downgrade.git
cd dynamips-downgrade
mise run build
```

The default build produces `dynamips`, `dynamips-core`, and
`dynamips-bindings`. The C++ binding is the C++23 module `dynamips`, imported
with `import dynamips;`; it does not expose a public C++ header.

Run the Go binding test, which builds the native debug libraries first:

```
mise run test
```

Run the C7200 Go example with an extracted local IOS image:

```
DYNAMIPS_IOS_IMAGE=/path/to/c7200-ios.bin mise run run:example
```

See [the example](go/examples/basic/README.md) for console and RAM options.

Format and lint the bindings separately:

```
mise run fmt:go
mise run lint:go
mise run lint:cpp
```

The C++ lint task builds the native module, runs cppcheck on the C/C++
embedding boundary, and runs clang-tidy on `bindings/*.cpp`. It requires
`clang-tidy`; mise provides cppcheck. The C++23 module partitions are checked
by the Clang build because xmake's compilation database does not currently
include their module commands for clang-tidy.

The Go package lives in `go/`. It initializes Dynamips without CLI options or
a TCP hypervisor and exposes VM, NIO, slot, and C7200 configuration bindings:

```go
runtime, err := dynamips.New()
if err != nil {
	return err
}

vm, err := runtime.CreateVM(dynamips.VMConfig{
	Name:       "router-1",
	InstanceID: 1,
	Platform:   "c7200",
})
if err != nil {
	return err
}

vm.Close()
return runtime.Close()
```

Inspect the local API with:

```
mise exec -- go doc github.com/tethux/dynamips-downgrade/go
mise exec -- go doc github.com/tethux/dynamips-downgrade/go/errs
```

The published Go API is at [pkg.go.dev](https://pkg.go.dev/github.com/tethux/dynamips-downgrade/go),
with [typed error documentation](https://pkg.go.dev/github.com/tethux/dynamips-downgrade/go/errs)
and a package example. The hosted pages update after this work is published.

The embedded runtime is process wide and can be initialized only once per
process. Close VM, NIO, and switch handles before closing the runtime.

### IOS integration and current performance

`go/ios_boot_test.go` boots a C7200 image through both the embedded Go API and
the old TCP hypervisor. It connects to each IOS console and runs `terminal
length 0`, `show version`, `show ip interface brief`, `configure terminal`,
`hostname paritytest`, and `end`. The test is opt in because Cisco images are
not distributed with this repository. For a local archive containing one `.bin`
or `.image` file:

```sh
mise exec -- xmake f -m debug -y
mise exec -- xmake build -y dynamips
DYNAMIPS_IOS_ARCHIVE=/path/to/c7200-ios.zip DYNAMIPS_IOS_RAM_MB=256 \
  mise exec -- go test ./go -run '^TestC7200IOSBootParity$' -count=1 -v -timeout=8m
```

On this machine, six local C7200 archives passed that command test on both
paths (one run per image, 2026-09-23). `IOS ready` is wall time from VM setup
to a usable privileged prompt. RSS is process resident memory after the IOS
commands, sampled peak every 100 ms; CPU is user plus system time through the
same point. Images ran sequentially. The embedded process includes the Go test
harness and an already initialized runtime. The legacy process is a separate
executable, and its startup includes launching the hypervisor, so these are
observations rather than an isolated ABI benchmark.

| C7200 image | RAM | IOS ready, embedded / legacy | CPU, embedded / legacy | RSS, embedded / legacy |
| --- | ---: | ---: | ---: | ---: |
| 12.4(25g) a3jk9s `.bin` | 512 MiB | 11.58 / 11.91 s | 11.47 / 11.70 s | 214.3 / 206.2 MiB |
| 12.4(24)T5 adventerprisek9 `.bin` | 256 MiB | 19.28 / 19.47 s | 19.18 / 19.30 s | 224.1 / 216.2 MiB |
| 12.4(24)T5 adventerprisek9 `.image` | 256 MiB | 11.14 / 11.73 s | 11.04 / 11.58 s | 224.9 / 216.2 MiB |
| 15.2(4)M7 adventerprisek9 `.bin` | 256 MiB | 23.69 / 25.37 s | 23.64 / 25.24 s | 270.5 / 262.1 MiB |
| 15.2(4)S6 adventerprisek9 `.image` | 256 MiB | 9.74 / 9.64 s | 9.59 / 9.45 s | 244.2 / 235.8 MiB |
| 15.3(3)XB12 adventerprisek9 `.image` | 256 MiB | 13.94 / 13.76 s | 13.85 / 13.62 s | 269.2 / 258.6 MiB |

The current measured memory cost is about 8 to 11 MiB more total RSS for the
embedded Go test process. Boot and CPU times were close in these single runs;
the slower embedded case was 0.18 s slower to the IOS prompt and the fastest
was 1.69 s faster. Repeat runs under controlled load before treating those
time differences as a speedup or slowdown.

An isolated ASan build also booted the 12.4(24)T5 image and completed the IOS
commands. Valgrind found 0 bytes left at exit and no file descriptor growth
after 300 VM create/delete and 300 UDP NIO create/delete cycles. The VTTY
worker is now joined at shutdown. Full UBSan runs are currently blocked by
existing amd64 JIT operations that trigger function pointer, shift, and null
member checks; this is an explicit limit of the sanitizer result.

Select another JIT backend during configuration:

```
xmake f --dynamips_arch=nojit
```

Build and install with:

```
mise run build
xmake install
```

To specify a different installation location run:

```
xmake install -o /target/path
```

### Releasing

- Update ChangeLog
- In common/dynamips.c update sw_version_tag with date
- Update RELEASE-NOTE
- Update xmake.lua
- Tag the release with `jj tag set`

### Useful Information 

Forum: https://gns3.com/community

Repository: https://github.com/GNS3/dynamips

Bugtracker: https://github.com/GNS3/dynamips/issues
