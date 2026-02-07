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

* **wasi-sdk** (>= 30) -- the Clang/LLVM toolchain targeting `wasm32-wasip2`.
  Download from <https://github.com/WebAssembly/wasi-sdk/releases>.
  Version 30+ is required for socket support via `wasi:sockets`.
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
cmake --build build --target eavmlib
```

After this you will have:

* `build/tools/packbeam/PackBEAM` -- the packaging tool
* `build/libs/estdlib/src/estdlib.avm` -- the standard library archive
* `build/libs/eavmlib/src/eavmlib.avm` -- the AtomVM extended library
  (required for `io:format`, `console` module, and the `atomvm` module's
  POSIX file NIFs)

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
`gen_server`, `io`, etc.), include `estdlib.avm` and `eavmlib.avm` when
packaging:

```sh
build/tools/packbeam/PackBEAM myapp.avm myapp.beam \
  build/libs/estdlib/src/estdlib.avm \
  build/libs/eavmlib/src/eavmlib.avm
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

## Console output

There are two ways to produce console output:

### `erlang:display/1`

Always available, writes to stderr using Erlang term formatting:

```erlang
erlang:display(hello_world).    %% prints: hello_world
erlang:display({ok, 42}).       %% prints: {ok,42}
```

### `io:format/1,2`

Requires `eavmlib.avm` to be bundled (for the `console` module). Uses the
standard Erlang IO protocol:

```erlang
io:format("Hello ~s! The answer is ~p~n", ["world", 42]).
```

`io:format` works because the default group leader is `self()`, which causes
`io:put_chars` to call the `console:print/1` NIF directly (`fprintf(stdout)`).

## File I/O

WASI supports file operations through the AtomVM POSIX NIFs. The WASI runtime
must be given explicit access to directories via `--dir=` flags.

### Example: writing and reading a file

```erlang
%% Grant access: wasmtime --dir=/tmp/mydir build-wasi/AtomVM.wasm -- ...

%% Write
{ok, Fd} = atomvm:posix_open("/tmp/mydir/output.txt",
                              [o_wronly, o_creat, o_trunc], 8#644),
{ok, _BytesWritten} = atomvm:posix_write(Fd, <<"Hello WASI!\n">>),
ok = atomvm:posix_close(Fd).

%% Read
{ok, Fd2} = atomvm:posix_open("/tmp/mydir/output.txt", [o_rdonly]),
{ok, Data} = atomvm:posix_read(Fd2, 4096),
ok = atomvm:posix_close(Fd2).
```

### Important notes

* **`posix_open/3` is required when using `o_creat`** -- the third argument is
  the file permission mode (e.g. `8#644`). Using `posix_open/2` with `o_creat`
  raises `badarg`.
* **`posix_write/2` returns `{ok, BytesWritten}`**, not bare `ok`.
* **`posix_readdir/1` returns `eof`** (not `error`) at end-of-directory.
* **WASI filesystem sandboxing** -- The WASI runtime only grants access to
  directories specified with `--dir=`. Paths must be reachable from a
  pre-opened directory.

### Directory listing

```erlang
{ok, Dir} = atomvm:posix_opendir("/tmp/mydir"),
loop(Dir).

loop(Dir) ->
    case atomvm:posix_readdir(Dir) of
        {ok, {dirent, _Inode, Name}} ->
            erlang:display(Name),
            loop(Dir);
        eof ->
            atomvm:posix_closedir(Dir)
    end.
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
* `io:format/1,2` for formatted output (requires `eavmlib.avm` bundled)
* File I/O via POSIX NIFs (`atomvm:posix_open/2,3`, `posix_read/2`,
  `posix_write/2`, `posix_close/1`)
* Directory operations (`atomvm:posix_opendir/1`, `posix_readdir/1`,
  `posix_closedir/1`)
* `file:get_cwd/0`
* Console port driver (`open_port({spawn, "console"}, [])`)
* Socket API (`socket:open/3`, `socket:connect/2`, `socket:close/1`, etc.)
  for localhost connections (external connections have known issues)

## Platform constraints

This is a WASI MVP (Minimum Viable Product) port. The following limitations
apply compared to the `generic_unix` platform:

| Feature | Status | Reason |
|---------|--------|--------|
| SMP / threads | Disabled | WASI MVP has no thread support |
| JIT compilation | Disabled | No `mmap`/`mprotect` in WASI |
| Networking (TCP/UDP) | Partial | Socket API compiles; external connections may hang (upstream wasmtime issue) |
| `mmap`-based file loading | Replaced with `read()` | No `mmap` in WASI |
| Port drivers (`dlopen`) | Not available | No dynamic linking in WASI |
| `epoll`/`select`/`poll` | Partial | Uses `poll()` for socket events; timers use `nanosleep` |
| MbedTLS / crypto | Not available | No SSL/TLS support |
| `erlang:localtime/0` | Returns UTC | WASI has no timezone database (`tzset` is a no-op) |

## Networking (experimental)

The WASI platform now includes the `socket` module (TCP/UDP sockets) via the
`wasi:sockets` interface. This is **experimental** and has known limitations:

### What works

* **Socket creation** -- `socket:open/3` for both TCP (stream) and UDP (dgram)
* **Localhost connections** -- Connections to `127.0.0.1` work correctly
* **Socket options** -- Basic options like `SO_REUSEADDR`, `SO_KEEPALIVE`, etc.
* **Non-blocking mode** -- WASI sockets are always non-blocking (handled transparently)

### Known issues

* **External connections hang** -- Connections to external IP addresses (e.g.,
  `54.208.3.199:80`) hang indefinitely. This appears to be an upstream issue
  with wasmtime's WASI Preview 2 socket implementation or wasi-sdk's POSIX
  socket shim. See [wasmtime#9849](https://github.com/bytecodealliance/wasmtime/issues/9849)
  for related issues.
* **Error code mapping** -- Some socket errors may report as `closed` instead
  of the specific error code (e.g., `econnrefused`).

### Running with network access

Enable networking in wasmtime:

```sh
wasmtime --dir=/ \
  -S inherit-network \
  -S tcp \
  -S udp \
  -S allow-ip-name-lookup \
  build-wasi/AtomVM.wasm -- myapp.avm
```

### Example

```erlang
-module(socket_test).
-export([start/0]).

start() ->
    {ok, Socket} = socket:open(inet, stream, tcp),
    %% Localhost connections work
    case socket:connect(Socket, #{family => inet, addr => {127,0,0,1}, port => 8080}) of
        ok -> io:format("Connected!~n");
        {error, econnrefused} -> io:format("No server running~n")
    end,
    socket:close(Socket).
```

## File layout

```
src/platforms/wasi/
├── cmake/
│   └── wasi-sdk.cmake              # CMake toolchain file for wasi-sdk (wasm32-wasip2)
├── lib/
│   ├── CMakeLists.txt              # Platform library build
│   ├── otp_socket_platform.c       # Socket platform support (supports_peek)
│   ├── otp_socket_platform.h       # Socket platform defines (SO_LINGER fallback)
│   ├── platform_defaultatoms.c     # Atom registration (X-macro)
│   ├── platform_defaultatoms.def   # Defines the "wasi" atom
│   ├── platform_defaultatoms.h     # Atom declarations (X-macro)
│   ├── platform_nifs.c             # Platform NIFs (atomvm:platform/0, socket, net)
│   ├── sys.c                       # WASI sys.h implementation with poll()-based events
│   ├── wasi_compat.h               # Shims for fcntl, connect, tzset, etc.
│   └── wasi_sys.h                  # Platform data structures with pollfd array
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

* **Socket support via poll()** -- WASI Preview 2 provides sockets through
  the `wasi:sockets` interface. We implement `sys_poll_events()` using `poll()`
  to wait for socket I/O events, and provide shims for `fcntl()` and `connect()`
  to handle WASI's always-non-blocking sockets.
