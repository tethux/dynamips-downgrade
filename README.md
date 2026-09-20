# Dynamips (Cisco Router Emulator)

## Overview

Authors of this document: Fabien Devaux, Christophe Fillot, MtvE, 
Gordon Russell, Jeremy Grossmann and Flávio J. Saraiva.

Converted to markdown format by Daniel Lintott.

This is a continuation of Dynamips, based on the last development version and 
improved with patches wrote by various people from the community. This fork was
named Dynamips-community up to the 0.2.8-community release and renamed to the 
original Dynamips on the 0.2.9 release.

You can compile two different versions of Dynamips with this code. Set the
`dynamips_code` xmake option to `stable` or `unstable`.

Unstable is the code which contains most of the development code, and is
in particular suitable for use on a 64 bit Mac. Unfortunately this has
proved to be unstable on other platforms.

Stable contains the same code as Unstable, minus some mips64 bit optimisations
and tcb code which seems to trigger instability on a number of platforms.
You should probably use stable unless you have a very good reason.

For more information on the how to use Dynamips see the README file

License: GNU GPLv2 only

### Direction of this fork

This fork is not an attempt to pretend the old Dynamips internals are modern.
The code is useful and proven, but it also has decades of global state, coupled
runtime and CLI behavior, old JIT assumptions, and interfaces that are hard to
embed safely. It is kind of bad by current standards, and the point of this
work is to put a small, honest boundary around it before changing more of it.

The next steps are deliberately incremental:

1. Grow the reusable core and its small C ABI without changing emulator behavior.
2. Add narrow C++ bindings that are also straightforward to consume from Go.
3. Use Dynamips as the compatibility and reference implementation while the new
   emulator is being built.
4. Once the new emulator works, replace the legacy internals with modern C++
   behind the same small boundary instead of carrying this architecture forever.

The original document authors, maintainers, and community contributors remain
credited here, in `MAINTAINERS`, and throughout `ChangeLog`. Special thanks to
Christophe Fillot for creating Dynamips and to GNS3 and its contributors for
maintaining and publishing the code under GPLv2. That is what makes this fork,
and the frankly shameless borrowing of a working emulator, possible.

### How to compile Dynamips

Dynamips uses xmake with Clang and C23. mise provides the normal project
commands and tool versions. xmake uses installed dependencies first and falls
back to its package repository when necessary.

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

The default build produces `dynamips`, `dynamips-core`, `dynamips-hello`, and
`nvram_export`. Run the linkage test with:

```
mise run test
```

Select the unstable implementation or another JIT backend during configuration:

```
xmake f --dynamips_code=unstable --dynamips_arch=nojit
```

Optional helper programs can also be enabled during configuration:

```
xmake f --build_udp_send=y --build_udp_recv=y
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
