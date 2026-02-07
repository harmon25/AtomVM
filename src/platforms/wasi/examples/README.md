# WASI Examples

This directory contains example programs demonstrating AtomVM's capabilities on the WASI platform.

## Organization

### `wasmtime/` - Basic Wasmtime Examples
These examples demonstrate core AtomVM features when running with wasmtime:

- **Core VM Tests**: `test_arithmetic.avm`, `test_lists.avm`, `test_processes.avm`, `test_binary.avm`, `test_maps.avm`, `test_recursion.avm`
- **I/O Tests**: `test_io_format.avm`, `test_file_io.avm`, `test_console_port.avm`
- **Socket Tests**: `test_socket_basic.avm`, `test_tcp_client.avm`, `test_tcp_local.avm`
- **Basic Demo**: `hello.erl` → `hello.avm`

### `elixir/` - Elixir on WASI Examples
Elixir examples demonstrating functional Elixir on WASI:

- `wasm_elixir_simple.ex` - Simple Erlang functions wrapped in Elixir syntax
- `wasm_demo.ex` - Comprehensive Elixir demo (lists, maps, pattern matching, recursion)
- `pure_erlang_demo.ex` - Elixir code using only Erlang stdlib (`:lists`, `:maps`)
- `simple_demo.ex` - Simple computation example

### `spin/` - Fermyon Spin Examples
Examples demonstrating AtomVM running on Spin (serverless WebAssembly framework):

- `spin_simple.erl` → `spin_simple.avm` - Pure computation (no I/O), works perfectly
- `spin_handler.erl` - Async I/O (doesn't work in Spin command trigger - no TTY available)

Running with Spin:
```bash
spin up  # Uses `spin.toml` in same directory
```

## Building Examples

All .avm files need to be rebuilt if recompiling .erl/.ex sources:

```bash
cd /path/to/atomvm-test

# Compile Erlang
erlc test_arithmetic.erl

# Package with estdlib + eavmlib (from native build)
build/tools/packbeam/PackBEAM test_arithmetic.avm test_arithmetic.beam \
  build/libs/estdlib/src/estdlib.avm \
  build/libs/eavmlib/src/eavmlib.avm
```

## Running with Wasmtime

**Basic:**
```bash
wasmtime --dir=. build-wasi/AtomVM.wasm -- test_arithmetic.avm
```

**With filesystem access:**
```bash
wasmtime --dir=/path/to/dir build-wasi/AtomVM.wasm -- test_file_io.avm
```

**With network (experimental - may hang on external connections):**
```bash
wasmtime --dir=. -S inherit-network -S tcp -S udp \
  build-wasi/AtomVM.wasm -- test_tcp_local.avm
```

*Note: External TCP connections currently hang due to upstream wasmtime issue. Localhost (127.0.0.1) works fine.*

## Running with Spin

1. Ensure Spin CLI is installed:
```bash
curl -fsSL https://spinframework.dev/downloads/install.sh | bash
curl -fsSL https://spinframework.dev/downloads/install.sh | bash
sudo mv ./spin /usr/local/bin/  # or add to PATH
```

2. Install command trigger plugin:
```bash
spin plugins install trigger-command
```

3. Create `spin.toml`:
```toml
spin_manifest_version = 2

[application]
name = "my-erlang-app"
version = "0.1.0"

[[trigger.command]]
component = "erlang-app"

[component.erlang-app]
source = "AtomVM.wasm"  # Path to AtomVM.wasm
files = [{ source = "spin_simple.avm", destination = "/test.avm" }]
```

4. Run:
```bash
spin up
```

## Source Files

The `.erl` and `.ex` files are provided for reference and recompilation. All `.avm` files are prebuilt for convenience.

To rebuild from source, use the native (not WASI) AtomVM build:
```bash
cmake -S . -B build
cmake --build build --target PackBEAM
cmake --build build --target estdlib
```