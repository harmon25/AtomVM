<!---
  Copyright 2025 AtomVM Contributors

  SPDX-License-Identifier: Apache-2.0 OR LGPL-2.1-or-later
-->

# AtomVM WASI Platform

This directory contains the WASI (WebAssembly System Interface) platform port
of AtomVM. It compiles the VM into a `wasm32-wasi` binary (`AtomVM.wasm`) that
can run Erlang and Elixir programs inside any WASI-compatible runtime such as
[Wasmtime](https://wasmtime.dev/),
[Wasmer](https://wasmer.io/), or
[Spin (Fermyon)](https://www.fermyon.com/spin).

## Prerequisites

* **wasi-sdk** (>= 20) -- the Clang/LLVM toolchain targeting `wasm32-wasip1`.
  Download from <https://github.com/WebAssembly/wasi-sdk/releases>.
* **CMake** (>= 3.13)
* **A WASI runtime** to execute the resulting binary. For example:
  ```
  curl https://wasmtime.dev/install.sh -sSf | bash
  ```
* **Erlang/OTP** (>= 26) -- needed to compile `.erl` files into `.beam` files.
* **PackBEAM** -- the native AtomVM tool for packaging `.beam` files into
  `.avm` archives. Built from the repository root (see below).

## Building

### 1. Build AtomVM.wasm

From the repository root:

```sh
export WASI_SDK_PATH=/path/to/wasi-sdk

cmake -S src/platforms/wasi -B build-wasi \
  -DCMAKE_TOOLCHAIN_FILE=cmake/wasi-sdk.cmake

cmake --build build-wasi
```

The output is `build-wasi/AtomVM.wasm` (~2.5 MB).

You can also pass `WASI_SDK_PATH` as a CMake variable instead of an
environment variable:

```sh
cmake -S src/platforms/wasi -B build-wasi \
  -DWASI_SDK_PATH=/path/to/wasi-sdk \
  -DCMAKE_TOOLCHAIN_FILE=cmake/wasi-sdk.cmake
```

### 2. Build the PackBEAM tool and standard library (native host)

A separate native build is required to produce the `PackBEAM` packaging tool
and the AtomVM Erlang standard library (`estdlib.avm`):

```sh
cmake -S . -B build
cmake --build build --target PackBEAM
cmake --build build --target estdlib
```

After this you will have:

* `build/tools/packbeam/PackBEAM` -- the packaging tool
* `build/libs/estdlib/src/estdlib.avm` -- the standard library archive

## Creating an .avm file

Write an Erlang module that exports `start/0`:

```erlang
%% hello.erl
-module(hello).
-export([start/0]).

start() ->
    erlang:display(hello_world),
    ok.
```

Compile and package it:

```sh
erlc hello.erl
build/tools/packbeam/PackBEAM hello.avm hello.beam
```

If your program uses standard library modules (e.g. `maps`, `lists`,
`gen_server`, `io`, etc.), include `estdlib.avm` when packaging:

```sh
build/tools/packbeam/PackBEAM myapp.avm myapp.beam build/libs/estdlib/src/estdlib.avm
```

## Running

Use any WASI runtime. With Wasmtime:

```sh
wasmtime --dir=. build-wasi/AtomVM.wasm -- hello.avm
```

The `--dir=.` flag grants the WASM module access to the current directory so it
can read the `.avm` file. Adjust the path as needed.

Expected output:

```
hello_world
Return value: ok
```

You can also load multiple `.avm` or `.beam` files:

```sh
wasmtime --dir=. build-wasi/AtomVM.wasm -- myapp.avm extra_lib.avm
```

Run with `-h` to see usage:

```sh
wasmtime build-wasi/AtomVM.wasm -- -h
```

## What works

The following VM features have been tested and work correctly on WASI:

* Integer, float, and big-integer arithmetic
* Lists (construction, concatenation, comprehensions, pattern matching)
* Binaries and bit strings (construction, pattern matching, `byte_size`,
  `binary_to_list`)
* Maps (creation, update syntax, `maps:get/2`, `maps:keys/1`, `maps:size/1`)
* Tuples and atoms
* Process spawning and message passing (`spawn/1`, `send`, `receive/after`)
* Recursion and tail-call optimization
* Pattern matching and guards
* Standard library modules when bundled (e.g. `erlang`, `maps`, `lists`)
* `erlang:display/1` for output

## Platform constraints

This is a WASI MVP (Minimum Viable Product) port. The following limitations
apply compared to the `generic_unix` platform:

| Feature | Status | Reason |
|---------|--------|--------|
| SMP / threads | Disabled | WASI MVP has no thread support |
| JIT compilation | Disabled | No `mmap`/`mprotect` in WASI |
| Networking (TCP/UDP) | Not available | WASI sockets proposal not yet stable |
| `mmap`-based file loading | Replaced with `read()` | No `mmap` in WASI |
| Port drivers (`dlopen`) | Not available | No dynamic linking in WASI |
| `epoll`/`select`/`poll` | Not available | `sys_poll_events` uses `nanosleep` |
| MbedTLS / crypto | Not available | No SSL/TLS support |
| `erlang:localtime/0` | Returns UTC | WASI has no timezone database (`tzset` is a no-op) |

## File layout

```
src/platforms/wasi/
├── cmake/
│   └── wasi-sdk.cmake              # CMake toolchain file for wasi-sdk
├── lib/
│   ├── CMakeLists.txt              # Platform library build
│   ├── platform_defaultatoms.c     # Atom registration (X-macro)
│   ├── platform_defaultatoms.def   # Defines the "wasi" atom
│   ├── platform_defaultatoms.h     # Atom declarations (X-macro)
│   ├── platform_nifs.c             # atomvm:platform/0 -> 'wasi'
│   ├── sys.c                       # WASI sys.h implementation
│   ├── wasi_compat.h               # Shims for missing wasi-libc functions
│   └── wasi_sys.h                  # Platform data structures
├── CMakeLists.txt                  # Top-level standalone build
├── main.c                          # Entry point
└── README.md                       # This file
```

## Design notes

* **Standalone CMake project** -- Like the ESP32 and Emscripten platforms,
  the WASI build is a self-contained CMake project under
  `src/platforms/wasi/` that pulls in `libAtomVM` via `add_subdirectory`.
  It is not integrated into the top-level `CMakeLists.txt`.

* **Mandatory `-Oz` optimization** -- The BEAM opcode dispatch function
  (`scheduler_entry_point` in `opcodesswitch.h`) is roughly 7800 lines of C.
  At `-O0`, this exceeds the WebAssembly 50,000 local variable limit. The
  build forces `-Oz` globally to keep the local count within spec.

* **No `tzset()`** -- wasi-libc omits `tzset()` because WASI has no timezone
  database. Since `libAtomVM` calls `tzset()` for `erlang:localtime/0`, we
  provide a no-op stub via `wasi_compat.h`, force-included into the libAtomVM
  build.

* **Single-threaded only** -- `AVM_DISABLE_SMP` is set unconditionally. All
  SMP-related code paths (locks, thread creation) are compiled out.
