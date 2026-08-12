# Spin HTTP Trigger Support for AtomVM

## Status: Implemented

AtomVM supports the `wasi:http/incoming-handler` interface, enabling Erlang/Elixir
HTTP handlers to run as Spin components.

## Architecture

```
HTTP Request (Spin)
        │
        ▼
┌─────────────────────────────┐
│  wasi:http/incoming-handler │  (WASI component model export)
│  exports_wasi_http_         │
│  incoming_handler_handle()  │
└─────────────┬───────────────┘
              │
              ▼
┌─────────────────────────────┐
│  wasi_http_handler.c        │  Extracts request → Erlang map
│                             │  Calls handler:handle/1
│                             │  Converts response map → WASI
└─────────────┬───────────────┘
              │
              ▼
┌─────────────────────────────┐
│  AtomVM BEAM Interpreter    │  Runs your Erlang/Elixir code
│  SpinHandler / spin_handler │
└─────────────────────────────┘
```

The component runs in **reactor mode**: AtomVM initializes once during module
instantiation (loading the `.avm` file), then the host calls `handle()` for
each incoming HTTP request.

## Quick Start

### 1. Build the HTTP component

```bash
# Ensure wasi-sdk is installed
export WASI_SDK_PATH=/path/to/wasi-sdk-30

# Build using the script
cd src/platforms/wasi/http
./build-component.sh

# Or manually with CMake
cmake -S src/platforms/wasi -B build-wasi \
    -DCMAKE_TOOLCHAIN_FILE=src/platforms/wasi/cmake/wasi-sdk.cmake \
    -DAVM_BUILD_HTTP_COMPONENT=ON
cmake --build build-wasi --target AtomVM_http
```

Output: `build-wasi/AtomVM_http.wasm`

### 2. Write an Erlang handler

Create `spin_handler.erl`:

```erlang
-module(spin_handler).
-export([handle/1]).

handle(#{method := Method, path := Path, body := Body}) ->
    case Path of
        <<"/">> ->
            #{status => 200,
              headers => [{<<"content-type">>, <<"text/plain">>}],
              body => <<"Hello from AtomVM!">>};
        <<"/echo">> ->
            #{status => 200,
              headers => [{<<"content-type">>, <<"application/octet-stream">>}],
              body => Body};
        _ ->
            #{status => 404,
              headers => [{<<"content-type">>, <<"text/plain">>}],
              body => <<"Not Found">>}
    end.
```

### 3. Compile to .avm

```bash
erlc spin_handler.erl

# Compile platform NIF stubs (required for spin_http, spin_kv, etc.)
erlc src/platforms/wasi/http/spin_http.erl \
     src/platforms/wasi/http/spin_kv.erl \
     src/platforms/wasi/http/spin_config.erl \
     src/platforms/wasi/http/spin_sqlite.erl \
     src/platforms/wasi/http/spin_postgres.erl

packbeam create app.avm spin_handler.beam \
    spin_http.beam spin_kv.beam spin_config.beam \
    spin_sqlite.beam spin_postgres.beam
```

Or use the build script: `examples/erlang/wasi/build_and_run.sh`

### 4. Run with Spin

Create `spin.toml`:

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
key_value_stores = ["default"]
allowed_outbound_hosts = ["https://*:*"]
```

```bash
spin up
curl http://localhost:3000/
# => Hello from AtomVM!
```

## Handler API

### Request Map

The handler receives a map with these keys:

| Key         | Type                                | Description                          |
|-------------|-------------------------------------|--------------------------------------|
| `method`    | `atom()`                            | `get`, `post`, `put`, `delete`, etc. |
| `path`      | `binary()`                          | Path with query, e.g. `<<"/foo?x=1">>` |
| `headers`   | `[{binary(), binary()}]`            | List of `{Name, Value}` tuples       |
| `body`      | `binary()`                          | Request body (empty binary if none)  |
| `authority` | `binary()` (optional)               | Host header / authority              |

### Response Format

Return a map with these keys:

| Key       | Type                     | Description                          |
|-----------|--------------------------|--------------------------------------|
| `status`  | `integer()`              | HTTP status code (e.g., 200)         |
| `headers` | `[{binary(), binary()}]` | Response headers                     |
| `body`    | `binary()`               | Response body                        |

Alternatively, return a plain `binary()` for a simple 200 OK text/plain response.

## File Structure

```
http/
├── wit/
│   ├── app.wit                    # WIT world definition (exports incoming-handler)
│   └── deps/                      # WASI interface dependencies
│       ├── cli/                   # wasi:cli@0.2.8
│       ├── clocks/                # wasi:clocks@0.2.8
│       ├── filesystem/            # wasi:filesystem@0.2.8
│       ├── http/                  # wasi:http@0.2.8 (handler.wit, types.wit, proxy.wit)
│       ├── io/                    # wasi:io@0.2.8 (streams, poll, error)
│       ├── random/                # wasi:random@0.2.8
│       └── sockets/               # wasi:sockets@0.2.8
├── generated/
│   ├── app.h                      # Generated C bindings (wit-bindgen 0.52.0)
│   ├── app.c                      # Generated adapter/glue code
│   └── app_component_type.o       # Component type metadata object
├── wasi_http_handler.h            # Handler API header
├── wasi_http_handler.c            # incoming-handler implementation
├── main_http.c                    # Reactor-mode initialization
├── build-component.sh             # Build script
└── README.md                      # This file
```

## Regenerating Bindings

If you modify `wit/app.wit` or update WIT dependencies:

```bash
# Requires: cargo install wit-bindgen-cli
./build-component.sh --regenerate-bindings
```

Or manually:

```bash
wit-bindgen c wit/ --out-dir generated/
```

## How It Works

1. **Initialization** (`main_http.c`): A constructor function runs during WASI
   module instantiation. It creates the AtomVM `GlobalContext`, loads `app.avm`
   from the WASI filesystem, and stores the context globally.

2. **Request handling** (`wasi_http_handler.c`): When the host calls
   `wasi:http/incoming-handler.handle`, we:
   - Extract method, path, headers, and body from WASI resources
   - Build an Erlang map on a fresh Context's heap
   - Call the handler's `handle/1` via `context_execute_loop`
   - Parse the returned Erlang response map
   - Send the response back through WASI's `response-outparam`

3. **Component model**: The `wasm32-wasip2` target directly produces a valid
   WASI component. No separate `wasm-tools component new` step is needed.

## Elixir Support

The handler also supports Elixir modules. Define `SpinHandler.handle/1`:

```elixir
defmodule SpinHandler do
  def handle(%{method: method, path: path, body: body}) do
    case path do
      "/" -> %{status: 200, headers: [{"content-type", "text/plain"}], body: "Hello from Elixir!"}
      _ -> %{status: 404, headers: [{"content-type", "text/plain"}], body: "Not Found"}
    end
  end
end
```

Compile and package:

```bash
elixirc --no-docs --no-debug-info spin_handler.ex

# Compile platform NIF stubs
erlc src/platforms/wasi/http/spin_http.erl \
     src/platforms/wasi/http/spin_kv.erl \
     src/platforms/wasi/http/spin_config.erl \
     src/platforms/wasi/http/spin_sqlite.erl \
     src/platforms/wasi/http/spin_postgres.erl

packbeam create app.avm Elixir.SpinHandler.beam \
    spin_http.beam spin_kv.beam spin_config.beam \
    spin_sqlite.beam spin_postgres.beam \
    estdlib.avm eavmlib.avm
```

The runtime tries `Elixir.SpinHandler` (Elixir) first, then `spin_handler`
(Erlang) as a fallback. See `examples/elixir/wasi/` for a full Elixir example
with a build script (`build_and_run.sh`).

## Limitations

- **Synchronous**: Each request blocks the single-threaded WASM instance.
  This is fine for Spin, which creates instances per-request.
- **No streaming**: The entire request body is buffered in memory before
  being passed to the handler. Response bodies are also fully buffered.
- **Module name**: The handler module must be named `spin_handler` (Erlang)
  or `SpinHandler` (Elixir) and export `handle/1`.
