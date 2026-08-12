# Erlang WASI Examples

Erlang examples for AtomVM running on the WASI platform.

## Files

| File                | Description                                        |
|---------------------|----------------------------------------------------|
| `hello.erl`         | Hello world -- minimal example using erlang:display |
| `spin_simple.erl`   | Simple computation running on Fermyon Spin          |
| `spin_handler.erl`  | HTTP request handler for Spin                      |
| `spin.toml`         | Spin manifest for the Erlang examples              |
| `build_and_run.sh`  | Build script: compile, package, and launch Spin    |

## Running with Wasmtime

```bash
# Compile and package
erlc hello.erl
build/tools/packbeam/PackBEAM hello.avm hello.beam

# Run
wasmtime --dir=. build-wasi/AtomVM.wasm -- hello.avm
```

## Running with Fermyon Spin

### Command Trigger (spin_simple.erl)

```bash
erlc spin_simple.erl
build/tools/packbeam/PackBEAM spin_simple.avm spin_simple.beam

# Install Spin command trigger plugin (one-time)
spin plugins install trigger-command

# Run
spin up --from spin.toml
```

### HTTP Handler (spin_handler.erl)

The easiest way is to use the build script:

```bash
./build_and_run.sh

# Test
curl http://localhost:3000/
curl http://localhost:3000/hello
curl -X POST -d "hello" http://localhost:3000/echo
curl http://localhost:3000/proxy    # outbound HTTP
```

Or manually:

```bash
# Compile handler + platform NIF stubs
erlc spin_handler.erl
erlc src/platforms/wasi/http/spin_http.erl \
     src/platforms/wasi/http/spin_kv.erl \
     src/platforms/wasi/http/spin_config.erl \
     src/platforms/wasi/http/spin_sqlite.erl \
     src/platforms/wasi/http/spin_postgres.erl

# Package (handler + NIF stubs + standard libraries)
build/tools/packbeam/PackBEAM app.avm spin_handler.beam \
    spin_http.beam spin_kv.beam spin_config.beam \
    spin_sqlite.beam spin_postgres.beam \
    build/libs/estdlib/src/estdlib.avm \
    build/libs/eavmlib/src/eavmlib.avm

cp build-wasi/AtomVM_http.wasm .
spin up --from spin.toml
```

> **Note**: The `spin_*.beam` platform NIF stubs must be included in the AVM
> pack. Without them, calls to `spin_http`, `spin_kv`, and other platform
> APIs will fail at runtime.

See `../../elixir/wasi/` for the Elixir versions of these examples.
