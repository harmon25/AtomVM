# AtomVM on Fermyon Spin - Status & Guide

## Status Overview

| Feature | Status | Notes |
|---------|--------|-------|
| **Command Trigger** | ✅ Working | Pure computation works perfectly |
| **HTTP Trigger** | ✅ Working | Full wasi:http/incoming-handler implementation |
| **Redis Trigger** | ❌ Not implemented | Requires specific adapters |

---

## ✅ HTTP Trigger (Recommended)

AtomVM implements the `wasi:http/incoming-handler` interface, enabling
Erlang HTTP handlers to run as Spin HTTP components.

### Quick Start

1. **Build the HTTP component**:
```bash
export WASI_SDK_PATH=/path/to/wasi-sdk-30
cmake -S src/platforms/wasi -B build-wasi \
    -DCMAKE_TOOLCHAIN_FILE=src/platforms/wasi/cmake/wasi-sdk.cmake
cmake --build build-wasi --target AtomVM_http
```

2. **Write your handler** (`spin_handler.erl`):
```erlang
-module(spin_handler).
-export([handle/1]).

handle(#{method := _Method, path := Path}) ->
    case Path of
        <<"/">> ->
            #{status => 200,
              headers => [{<<"content-type">>, <<"text/plain">>}],
              body => <<"Hello from AtomVM on Spin!">>};
        _ ->
            #{status => 404,
              headers => [{<<"content-type">>, <<"text/plain">>}],
              body => <<"Not Found">>}
    end.
```

3. **Compile to .avm**:
```bash
erlc spin_handler.erl
packbeam create app.avm spin_handler.beam
```

4. **Create `spin.toml`**:
```toml
spin_manifest_version = 2

[application]
name = "atomvm-http"
version = "0.1.0"

[[trigger.http]]
route = "/..."
component = "atomvm"

[component.atomvm]
source = "AtomVM_http.wasm"
files = [{ source = "app.avm", destination = "/app.avm" }]
```

5. **Run**:
```bash
spin up
curl http://localhost:3000/
# => Hello from AtomVM on Spin!
```

### Handler API

**Request map keys:**

| Key         | Type                     | Description                          |
|-------------|--------------------------|--------------------------------------|
| `method`    | `atom()`                 | `get`, `post`, `put`, `delete`, etc. |
| `path`      | `binary()`               | Path with query string               |
| `headers`   | `[{binary(), binary()}]` | Request headers                      |
| `body`      | `binary()`               | Request body                         |
| `authority` | `binary()` (optional)    | Host/authority                       |

**Response map keys:**

| Key       | Type                     | Description                          |
|-----------|--------------------------|--------------------------------------|
| `status`  | `integer()`              | HTTP status code                     |
| `headers` | `[{binary(), binary()}]` | Response headers                     |
| `body`    | `binary()`               | Response body                        |

You can also return a plain `binary()` as a shorthand for 200 OK text/plain.

### Testing with wasmtime serve

```bash
# No Spin needed — wasmtime works directly
wasmtime serve --dir=. build-wasi/AtomVM_http.wasm
curl http://localhost:8080/
```

---

## ✅ Command Trigger (Also Working)

The command trigger works for computation-only workloads.

### Demonstration

1. **Build AtomVM**:
```bash
export WASI_SDK_PATH=/path/to/wasi-sdk-30
cmake -S src/platforms/wasi -B build-wasi \
    -DCMAKE_TOOLCHAIN_FILE=src/platforms/wasi/cmake/wasi-sdk.cmake
cmake --build build-wasi --target AtomVM
```

2. **Create a simple Erlang module**:
```erlang
-module(spin_simple).
-export([start/0]).

start() ->
    X = 2 + 2,
    Y = X * 10,
    case Y of 40 -> ok; _ -> error end.
```

3. **Compile and package**:
```bash
erlc spin_simple.erl
packbeam create spin_simple.avm spin_simple.beam
```

4. **Create Spin application**:
```toml
# spin.toml
spin_manifest_version = 2

[application]
name = "atomvm-cmd-demo"
version = "0.1.0"

[[trigger.command]]
component = "atomvm"

[component.atomvm]
source = "AtomVM.wasm"
files = [{ source = "spin_simple.avm", destination = "/test.avm" }]
```

5. **Run**:
```bash
spin plugins install trigger-command  # once
spin up
# Output: Return value: ok
```

---

## Example Project Structure

```
atomvm-spin-demo/
├── spin.toml              # Spin manifest
├── AtomVM_http.wasm       # HTTP trigger component
├── app.avm                # Your compiled Erlang handler
└── spin_handler.erl       # Source code
```

---

## Resources

- [HTTP Implementation Details](../http/README.md)
- [WASI Platform Documentation](../WASI.md)
- [WASI HTTP Spec](https://github.com/WebAssembly/wasi-http)
- [Spin Documentation](https://spinframework.dev/)
- [Component Model](https://component-model.bytecodealliance.org/)
