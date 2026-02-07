# Erlang WASI Examples

Erlang examples for AtomVM running on the WASI platform.

## Files

| File               | Description                                        |
|--------------------|----------------------------------------------------|
| `hello.erl`        | Hello world -- minimal example using erlang:display |
| `spin_simple.erl`  | Simple computation running on Fermyon Spin          |
| `spin_handler.erl` | HTTP request handler for Spin                      |
| `spin.toml`        | Spin manifest for the Erlang examples              |

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

```bash
erlc spin_handler.erl
build/tools/packbeam/PackBEAM app.avm spin_handler.beam \
    build/libs/estdlib/src/estdlib.avm \
    build/libs/eavmlib/src/eavmlib.avm

cp build-wasi/AtomVM_http.wasm .
spin up --from spin.toml

# Test
curl http://localhost:3000/
curl http://localhost:3000/hello
curl -X POST -d "hello" http://localhost:3000/echo
```

See `../../elixir/wasi/` for the Elixir versions of these examples.
