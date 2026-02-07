# AtomVM WASI Platform

This directory contains the WASI (WebAssembly System Interface) platform port of AtomVM. It compiles the VM into a WebAssembly binary that can run Erlang/Elixir programs in any WASI-compatible runtime.

## Overview

The WASI platform allows AtomVM to run as a WebAssembly module, enabling Erlang/Elixir code to execute in:
- **Wasmtime** - Fast, secure WebAssembly runtime
- **Wasmer** - Universal WebAssembly runtime  
- **Fermyon Spin** - Serverless WebAssembly framework (with limitations)
- **SpinKube** - Kubernetes operator for Spin apps
- **Browser environments** (via WASI polyfills)

## Quick Start

### Prerequisites

- **wasi-sdk** (>= 30) - Download from https://github.com/WebAssembly/wasi-sdk/releases
- **CMake** (>= 3.13)
- **A WASI runtime** (e.g., Wasmtime: `curl https://wasmtime.dev/install.sh -sSf | bash`)
- **Erlang/OTP** (>= 26) for compiling .erl files
- **PackBEAM** - Build from AtomVM repository

### Building AtomVM.wasm

```bash
export WASI_SDK_PATH=/path/to/wasi-sdk-30

cmake -S src/platforms/wasi -B build-wasi \
  -DCMAKE_TOOLCHAIN_FILE=cmake/wasi-sdk.cmake

cmake --build build-wasi
```

Output: `build-wasi/AtomVM.wasm` (~3 MB)

### Build PackBEAM Tool (native)

```bash
cmake -S . -B build
cmake --build build --target PackBEAM
cmake --build build --target estdlib
cmake --build build --target eavmlib
```

### Running Erlang Code

```bash
# Compile Erlang
erlc myapp.erl

# Package into .avm
build/tools/packbeam/PackBEAM myapp.avm myapp.beam \
  build/libs/estdlib/src/estdlib.avm \
  build/libs/eavmlib/src/eavmlib.avm

# Run with Wasmtime
wasmtime --dir=. build-wasi/AtomVM.wasm -- myapp.avm
```

## Features

### ✅ Fully Supported

- Integer, float, and big-integer arithmetic
- Lists, tuples, maps, binaries
- Pattern matching and guards
- Process spawning and message passing
- Recursion and tail-call optimization
- File I/O via POSIX NIFs (with `--dir=` flag)
- Socket API (TCP/UDP) on Wasmtime
- Elixir compilation (uses Erlang stdlib)

### ⚠️ Limitations

| Feature | Status | Notes |
|---------|--------|-------|
| SMP/Threads | ❌ Disabled | WASI MVP has no threads |
| JIT | ❌ Disabled | No mmap/mprotect in WASI |
| Console I/O | ⚠️ Limited | Works with wasmtime, limited on Spin |
| Raw Sockets | ⚠️ Partial | Works on wasmtime with `-S tcp`, hangs on external connections |
| Spin HTTP | ❌ Not supported | Would need wasi:http interface implementation |

## Platform-Specific Details

### Wasmtime

Full-featured WASI runtime with socket support:

```bash
# Basic execution
wasmtime --dir=. AtomVM.wasm -- app.avm

# With network access
wasmtime --dir=. -S inherit-network -S tcp -S udp AtomVM.wasm -- app.avm

# Allow DNS resolution
wasmtime --dir=. -S inherit-network -S tcp -S udp -S allow-ip-name-lookup AtomVM.wasm -- app.avm
```

### Fermyon Spin

**Status**: Command trigger works ✅, HTTP trigger requires component model ⚠️

**Command Trigger** (Working):
- ✅ Runs Erlang/Elixir code successfully
- ✅ Pure computation (math, pattern matching, recursion)
- ✅ Process spawning and message passing
- ✅ File I/O support
- ❌ No stdin/stdout - `io:format` doesn't produce visible output
- ❌ No CLI arguments - auto-detection of `/test.avm`, `/app.avm`, `./app.avm`

**HTTP Trigger** (Requires Additional Work):
- ⚠️ Requires WASI component model implementation
- ⚠️ See `src/platforms/wasi/http/` for experimental work
- See [docs/SPIN_DEMO.md](docs/SPIN_DEMO.md) for full status

#### Quick Spin Demo (Command Trigger)

```bash
# Install Spin
curl -fsSL https://spinframework.dev/downloads/install.sh | bash

# Install command trigger plugin
spin plugins install trigger-command

# Create spin.toml
cat > spin.toml << 'EOF'
spin_manifest_version = 2

[application]
name = "erlang-app"
version = "0.1.0"

[[trigger.command]]
component = "erlang-app"

[component.erlang-app]
source = "AtomVM.wasm"
files = [{ source = "myapp.avm", destination = "/test.avm" }]
EOF

# Run
spin up
```

**Note**: Spin's command trigger doesn't pass CLI arguments. AtomVM automatically looks for AVM files at `/test.avm`, `/app.avm`, or `./app.avm` when no arguments are provided.

## File Structure

```
src/platforms/wasi/
├── cmake/
│   └── wasi-sdk.cmake          # CMake toolchain file
├── lib/
│   ├── CMakeLists.txt          # Platform library build
│   ├── otp_socket_platform.c   # Socket platform hooks
│   ├── otp_socket_platform.h   # Socket platform defines
│   ├── platform_nifs.c         # Platform NIFs
│   ├── sys.c                   # System implementation (poll, time, files)
│   ├── wasi_compat.h           # Compatibility shims
│   └── wasi_sys.h              # Platform data structures
├── examples/                   # Example programs
│   ├── spin/                   # Spin-specific examples
│   └── wasmtime/               # Wasmtime examples
├── CMakeLists.txt              # Top-level build
├── main.c                      # Entry point
├── README.md                   # This file
└── WASI.md                     # Platform documentation
```

## Architecture

### WASI Target: wasm32-wasip2

We target WASI Preview 2 (wasip2) which provides:
- Component Model support
- Socket APIs via `wasi:sockets`
- Better POSIX compatibility

### Event Loop

The WASI platform implements a `poll()`-based event loop for socket I/O:

```c
void sys_poll_events(GlobalContext *glb, int timeout_ms)
{
    // Uses poll() on socket file descriptors
    // Handles select events for async I/O
}
```

### Socket Compatibility

WASI sockets are always non-blocking. We provide shims in `wasi_compat.h`:

```c
// fcntl returns success for O_NONBLOCK (already non-blocking)
// connect uses poll() to wait for completion
```

## Examples

### Hello World

```erlang
-module(hello).
-export([start/0]).

start() ->
    erlang:display(hello_world),
    ok.
```

### File I/O

```erlang
-module(file_demo).
-export([start/0]).

start() ->
    {ok, Fd} = atomvm:posix_open("/tmp/output.txt",
                                  [o_wronly, o_creat, o_trunc],
                                  8#644),
    {ok, _} = atomvm:posix_write(Fd, <<"Hello WASI!\n">>),
    ok = atomvm:posix_close(Fd),
    ok.
```

Run: `wasmtime --dir=/tmp build-wasi/AtomVM.wasm -- file_demo.avm`

### TCP Client (Wasmtime only)

```erlang
-module(tcp_client).
-export([start/0]).

start() ->
    {ok, Socket} = socket:open(inet, stream, tcp),
    ok = socket:connect(Socket, #{family => inet,
                                  addr => {127,0,0,1},
                                  port => 8080}),
    socket:send(Socket, <<"Hello\n">>),
    {ok, Data} = socket:recv(Socket, 0, 5000),
    erlang:display(Data),
    socket:close(Socket),
    ok.
```

Run: `wasmtime --dir=. -S inherit-network -S tcp AtomVM.wasm -- tcp_client.avm`

### Elixir on WASI

```elixir
defmodule WasmDemo do
  def start do
    numbers = [1, 2, 3, 4, 5]
    doubled = :lists.map(fn x -> x * 2 end, numbers)
    sum = :lists.foldl(fn x, acc -> x + acc end, 0, numbers)
    IO.puts("Sum: #{sum}")
    :ok
  end
end
```

**Note**: Elixir code uses Erlang stdlib functions (`:lists`, etc.) since the Elixir stdlib isn't bundled.

## Troubleshooting

### "Failed load module: init.beam"

This is a benign warning. AtomVM tries to load an optional `init.beam` module. Your app should still run.

### "no .avm or .beam files specified"

When running on Spin, ensure:
1. AVM file is embedded in `spin.toml` via `files` directive
2. File is at one of the default paths: `/test.avm`, `/app.avm`, `./app.avm`

### Socket connections hang

External TCP connections may hang on wasmtime. This is an upstream issue with wasi-libc's socket implementation. Localhost connections (127.0.0.1) work fine.

### Console output not showing

On Spin's command trigger, `io:format` and `erlang:display` may not produce visible output. The return value (`Return value: ok/error`) indicates success/failure.

## Known Issues

1. **Upstream socket bug**: External TCP connections hang (wasmtime issue)
2. **Spin TTY**: No console I/O in command trigger mode
3. **Socket errors**: Some socket errors report as `closed` instead of specific codes

## Contributing

To extend WASI platform support:

1. **HTTP trigger for Spin**: Implement `wasi:http/incoming-handler` interface
2. **Better socket error handling**: Map WASI-specific errors properly
3. **Elixir stdlib**: Bundle Elixir standard library for full Elixir support
4. **Component model**: Export AtomVM as a component with custom interfaces

## References

- [WASI Specification](https://github.com/WebAssembly/WASI)
- [Wasmtime Documentation](https://docs.wasmtime.dev/)
- [Spin Documentation](https://spinframework.dev/)
- [AtomVM Documentation](https://www.atomvm.net/)

## License

Apache-2.0 OR LGPL-2.1-or-later (same as AtomVM)