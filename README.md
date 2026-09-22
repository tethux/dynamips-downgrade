# Dynamips embedding experiment

This fork gives Tethux a typed C, C++23, and Go API for the stable Dynamips
emulator. The public Go package is at the repository root, like
[`shitnet`](https://github.com/tethux/shitnet). The old TCP hypervisor remains
as a separate target for behavior comparisons; the embedding library does not
link against its command handlers. The required, deferred, and legacy-only API
scope lives in [`bindings/schema/`](bindings/schema/).

This is an experimental, AI-assisted learning project. AI agents helped with
the binding backfill, refactor, and tests. I am using it to learn the emulator
and improve my development workflow. I do not claim this work is better than
the years of engineering behind Dynamips. I am having fun, and my long-term
goal is to rewrite most of the code into a smaller emulator containing only
what I need.

Christophe Fillot created Dynamips. Fabien Devaux, MtvE, Gordon Russell,
Jeremy Grossmann, Flávio J. Saraiva, GNS3, and other contributors maintained
and extended it. Daniel Lintott converted an earlier README to Markdown. The
license is GPLv2 only; see [`LICENSE`](LICENSE).

## Build and test

The project uses xmake with Clang, C23, and C++23. Install libelf and libpcap
development packages, then run:

```sh
mise run build
mise run test
mise run lint:go
mise run lint:cpp
```

`mise run test` builds the native debug libraries and runs the C++ module and
Go tests. `mise run lint:cpp` runs cppcheck on the new C/C++ embedding boundary
and clang-tidy on `bindings/*.cpp`. It requires `clang-tidy`; mise provides
cppcheck. The C++23 module partitions are checked by the Clang build because
xmake's compilation database does not currently include their module commands
for clang-tidy. xmake compiles with 14 parallel jobs by default here; set
`DYNAMIPS_BUILD_JOBS` to change the task setting. Use `mise run fmt:go` for Go
formatting.

## Go API

The Go package initializes Dynamips without CLI options or a TCP hypervisor.
It exposes VM lifecycle and configuration, cards, NIO, and C7200 settings.
The embedded runtime is process wide and can be initialized only once. Close
VM, NIO, and switch handles before closing it.

```go
runtime, err := dynamips.New()
if err != nil {
    return err
}
vm, err := runtime.CreateVM(dynamips.VMConfig{
    Name: "router-1", InstanceID: 1, Platform: "c7200",
})
if err != nil {
    return err
}
vm.Close()
return runtime.Close()
```

Inspect [the Go API](https://pkg.go.dev/github.com/tethux/dynamips-downgrade)
and [typed errors](https://pkg.go.dev/github.com/tethux/dynamips-downgrade/errs)
locally with:

```sh
mise exec -- go doc github.com/tethux/dynamips-downgrade
mise exec -- go doc github.com/tethux/dynamips-downgrade/errs
```

The hosted pages update after this work is published. The
[runnable C7200 example](examples/basic/README.md) takes a local IOS image or
zip archive and exposes its console. On this laptop, run:

```sh
mise run run:example
```

The task defaults to
`~/gns3Imgs/c7200-adventerprisek9-mz.124-24.T5.zip`. Set
`DYNAMIPS_IOS_IMAGE=/path/to/another-image-or-archive` to override it. The
same terminal opens the IOS console. Type commands there; press Ctrl+] and
enter `quit` when finished.

## IOS integration and current performance

[`ios_boot_test.go`](ios_boot_test.go) boots a C7200 image through both the
embedded Go API and the old TCP hypervisor. It runs `terminal length 0`,
`show version`, `show ip interface brief`, `configure terminal`,
`hostname paritytest`, and `end` on each IOS console. The test is opt in because
Cisco images are not distributed here:

```sh
DYNAMIPS_IOS_ARCHIVE=/path/to/c7200-ios.zip DYNAMIPS_IOS_RAM_MB=256 \
  mise run test:ios
```

On this laptop, the archive is at
`/home/veya/gns3Imgs/c7200-adventerprisek9-mz.124-24.T5.zip`.

Six local C7200 archives passed on both paths on 2026-09-23. `IOS ready` is
wall time from VM setup to a usable privileged prompt. CPU is user plus
system time through the command checks. RSS is total process resident memory
at that point, with peak sampled every 100 ms. Images ran sequentially, one
run each. The embedded process includes the Go test harness and an already
initialized runtime; the legacy figure is a separate executable whose startup
includes launching the hypervisor. These are observations, not an isolated
ABI benchmark.

| C7200 image | RAM | IOS ready, embedded / legacy | CPU, embedded / legacy | RSS, embedded / legacy |
| --- | ---: | ---: | ---: | ---: |
| 12.4(25g) a3jk9s `.bin` | 512 MiB | 11.58 / 11.91 s | 11.47 / 11.70 s | 214.3 / 206.2 MiB |
| 12.4(24)T5 adventerprisek9 `.bin` | 256 MiB | 19.28 / 19.47 s | 19.18 / 19.30 s | 224.1 / 216.2 MiB |
| 12.4(24)T5 adventerprisek9 `.image` | 256 MiB | 11.14 / 11.73 s | 11.04 / 11.58 s | 224.9 / 216.2 MiB |
| 15.2(4)M7 adventerprisek9 `.bin` | 256 MiB | 23.69 / 25.37 s | 23.64 / 25.24 s | 270.5 / 262.1 MiB |
| 15.2(4)S6 adventerprisek9 `.image` | 256 MiB | 9.74 / 9.64 s | 9.59 / 9.45 s | 244.2 / 235.8 MiB |
| 15.3(3)XB12 adventerprisek9 `.image` | 256 MiB | 13.94 / 13.76 s | 13.85 / 13.62 s | 269.2 / 258.6 MiB |

The embedded Go test process used about 8 to 11 MiB more total RSS in those
runs. Boot and CPU times were close. Repeat under controlled load before
interpreting their small differences as a speedup or slowdown.

An isolated ASan build also booted the 12.4(24)T5 image and completed the IOS
commands. Valgrind found 0 bytes left at exit and no file descriptor growth
after 300 VM create/delete and 300 UDP NIO create/delete cycles. Full UBSan
runs are currently blocked by existing amd64 JIT operations that trigger
function pointer, shift, and null member checks.
