# Spin HTTP Examples

HTTP handler examples for AtomVM running on Fermyon Spin.

## Erlang Example

```bash
# Compile the handler
erlc spin_handler.erl

# Package into .avm (include estdlib for io, lists, etc.)
packbeam create app.avm spin_handler.beam \
    build/libs/estdlib/src/estdlib.avm \
    build/libs/eavmlib/src/eavmlib.avm

# Copy the component and run
cp build-wasi/AtomVM_http.wasm .
spin up --from spin.toml

# Test
curl http://localhost:3000/
curl http://localhost:3000/hello
curl http://localhost:3000/info
curl http://localhost:3000/json
curl -X POST -d "hello" http://localhost:3000/echo
```

## Elixir Example

```bash
# Compile the handler
elixirc --no-docs --no-debug-info spin_handler.ex

# Package into .avm
# The Elixir module compiles to Elixir.SpinHandler.beam
packbeam create app.avm Elixir.SpinHandler.beam \
    build/libs/estdlib/src/estdlib.avm \
    build/libs/eavmlib/src/eavmlib.avm

# Copy the component and run
cp build-wasi/AtomVM_http.wasm .
spin up --from spin_elixir.toml

# Test
curl http://localhost:3000/
curl http://localhost:3000/hello
curl http://localhost:3000/info
curl http://localhost:3000/json
curl http://localhost:3000/compute
curl -X POST -d "hello" http://localhost:3000/echo
```

## Testing with wasmtime (no Spin needed)

```bash
# Place app.avm in the current directory, then:
wasmtime serve --dir=. build-wasi/AtomVM_http.wasm
curl http://localhost:8080/
```

## Handler API

Both Erlang and Elixir handlers implement the same contract:

- **Erlang**: `spin_handler:handle/1`
- **Elixir**: `SpinHandler.handle/1`

The C runtime tries both module names, so you can use either language.

### Request Map

```elixir
%{
  method:    :get | :post | :put | :delete | ...,
  path:      <<"/path?query=string">>,
  headers:   [{"name", "value"}, ...],
  body:      <<"request body">>,
  authority: <<"host:port">>       # optional
}
```

### Response Map

```elixir
%{
  status:  200,
  headers: [{"content-type", "text/plain"}],
  body:    <<"response body">>
}
```

Or return a plain binary for a simple 200 OK text/plain response.

## Files

| File                | Description                          |
|---------------------|--------------------------------------|
| `spin_handler.erl`  | Erlang HTTP handler example          |
| `spin_handler.ex`   | Elixir HTTP handler example          |
| `spin_simple.erl`   | Simple command trigger example       |
| `spin.toml`         | Spin manifest (Erlang)               |
| `spin_elixir.toml`  | Spin manifest (Elixir)               |
