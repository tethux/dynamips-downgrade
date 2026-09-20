# Dynamips (Cisco Router Emulator)

[![Build Status](https://github.com/GNS3/dynamips/actions/workflows/codeql.yml/badge.svg)](https://github.com/GNS3/dynamips/actions/workflows/codeql.yml)

## Overview

Authors of this document: Fabien Devaux, Christophe Fillot, MtvE, 
Gordon Russell, Jeremy Grossmann and Flávio J. Saraiva.

Converted to markdown format by Daniel Lintott.

This is a continuation of Dynamips, based on the last development version and 
improved with patches wrote by various people from the community. This fork was
named Dynamips-community up to the 0.2.8-community release and renamed to the 
original Dynamips on the 0.2.9 release.

You can compile two different versions of Dynamips with this code.
Edit the Makefile to set the flags to suit your environment.
One of the flags, DYNAMIPS_CODE, can be "stable" or "unstable".

Unstable is the code which contains most of the development code, and is
in particular suitable for use on a 64 bit Mac. Unfortunately this has
proved to be unstable on other platforms.

Stable contains the same code as Unstable, minus some mips64 bit optimisations
and tcb code which seems to trigger instability on a number of platforms.
You should probably use stable unless you have a very good reason.

For more information on the how to use Dynamips see the README file

License: GNU GPLv2 only

### How to compile Dynamips

Dynamips uses xmake with Clang and C23. xmake uses installed dependencies first
and falls back to its package repository when necessary.

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
- xmake

#### Compiling (Linux/Mac)

Either download and extract a source tarball from the releases page or clone the
Git repository using:

```
git clone https://github.com/GNS3/dynamips.git
cd dynamips
xmake f -m release
xmake
```

The default build produces `dynamips`, `dynamips-core`, `dynamips-hello`, and
`nvram_export`. Run the linkage test with:

```
xmake test
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
xmake
xmake install
```

To specify a different installation location run:

```
xmake install -o /target/path
```

### Releasing

* Update ChangeLog
* In common/dynamips.c update sw_version_tag with date
* Update RELEASE-NOTE
* Update xmake.lua
* git tag the release

### Useful Information 

Forum: https://gns3.com/community

Repository: https://github.com/GNS3/dynamips

Bugtracker: https://github.com/GNS3/dynamips/issues
