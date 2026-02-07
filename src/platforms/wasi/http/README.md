# Spin HTTP Trigger Support for AtomVM

## Status: Work in Progress

This document outlines the approach for implementing Spin HTTP trigger support using the WASI component model.

## Current Status

✅ **Completed**:
- Component model bindings generated via `wit-bindgen c`
- Simple component skeleton created
- Toolchain installed: `wasm-tools`, `wit-bindgen-cli`

⚠️ **Work in Progress**:
- Full `wasi:http/incoming-handler` implementation
- Component adapter integration
- Spin-compatible HTTP response generation

## Component Model Overview

The WASI component model allows us to:

1. Define interfaces in WIT (WebAssembly Interface Type) language
2. Generate language-specific bindings (C for AtomVM)
3. Create components that can be hosted by Spin

### Simple Component Structure

```
src/platforms/wasi/http/
├── wit/
│   └── app.wit              # WIT interface definition
├── host.h                   # Generated header
├── host.c                   # Generated adapter code
├── host_impl.c              # Our implementation
└── build-component.sh         # Build script
```

## Full HTTP Handler Implementation

To implement full `wasi:http/incoming-handler` support, we need:

### 1. WIT Dependencies Structure

```wit
wit/
├── app.wit                 # Our component definition
└── deps/
    └── wasi/
        └── http.wasm        # WIT dependencies (or .wit files)
```

The WIT file would be:

```wit
package atomvm:http

world app {
  export wasi:http/incoming-handler
}
```

This exports the standard `wasi:http` interface.

### 2. HTTP Handler Interface

The `wasi:http/incoming-handler` interface requires:

```c
// Generated signature (simplified)
void handle_incoming_request(
    incoming_request_t *request,
    response_outparam_t response
);
```

Where:
- `incoming_request`: Contains method, path, headers, body
- `response_outparam`: Used to send the response

### 3. Implementation Pattern

```c
void handle_incoming_request(
    incoming_request_t *request,
    response_outparam_t response_out
) {
    // 1. Extract request data
    method_t method = request->method();
    path_with_query_t path = request->path_with_query();
    headers_t headers = request->headers();
    body_t body;
    body = request->consume();

    // 2. Create response
    headers_t response_headers = make_headers();
    set_header(response_headers, "content-type", "text/plain");
    outgoing_response_t response = make_response(200, response_headers);
    outgoing_body_t resp_body = response.body();

    // 3. Write response body (from AtomVM)
    const char *atomvm_response = "Hello from AtomVM!";
    stream_write(resp_body, atomvm_response, strlen(atomvm_response));
    finish_body(resp_body, NULL);

    // 4. Send response
    response_outparam_set(response_out, response);
}
```

## Challenges & Solutions

### Challenge 1: WIT Dependency Resolution

**Issue**: `wit-bindgen c` needs all WASI dependencies properly structured.

**Solution Options**:
1. Copy WIT packages from `wasi-http` and `wasi-cli` repos
2. Use pre-compiled `.wasm` files for dependencies
3. Package as `wit` component and resolve dynamically

**Current Approach**: Use `wit/deps/wasi-http/wit/` directory structure.

### Challenge 2: Component Adapter Layer

**Issue**: AtomVM.wasm is a core module; Spin expects component model.

**Solution**: Use `wasm-tools component new` with adapters:

```bash
# Create component from core module
wasm-tools component new \
    --adapt=/path/to/wasi_snapshot_preview1.reactor.wasm \
    build-wasi/AtomVM.wasm \
    -o AtomVM-http.wasm
```

The `wasi_snapshot_preview1.reactor.wasm` adapter converts between core module and component model.

### Challenge 3: Resource Management

**Issue**: HTTP requests/responses are resources that need proper lifetime management.

**Solution**: Follow WASI resource patterns:
- Resources have refcounts
- Use `_borrow` for temporary access
- Use `_drop` to release references
- Generated bindings will handle this

## Recommended Implementation Path

### Phase 1: Simple Component (Current) ✅

- Create minimal component that exports a simple interface
- Get component building with `wasm-tools component new`
- Test with `wasmtime serve`

### Phase 2: Component Adapter Integration (Next)

- Obtain the HTTP adapter (wasi_snapshot_preview1 adapter)
- Create component from AtomVM.wasm
- Handle `canonical_abi_realloc` for string management

### Phase 3: Full HTTP Interface Implementation

- Set up proper WIT dependencies
- Generate HTTP bindings with `wit-bindgen c`
- Implement `handle_incoming_request`
- Integrate with AtomVM request/response handling

### Phase 4: Spin Integration

- Configure `spin.toml` for HTTP component
- Test HTTP requests
- Handle common HTTP patterns (routing, headers, streaming)

## Spin Configuration Example

For a component-based HTTP handler, `spin.toml` would be:

```toml
spin_manifest_version = 2

[application]
name = "atomvm-http"
version = "0.1.0"

[[trigger.http]]
route = "/..."
component = "atomvm"

[component.atomvm]
source = "AtomVM-http.wasm"
```

## Testing

### With Wasmtime (component model aware runtime):

```bash
wasmtime serve -S http AtomVM-http.wasm
```

### With Spin:

```bash
spin up
curl http://localhost:3000/
```

## Alternative: Simplified Approach

Given the complexity of full `wasi:http` implementation, consider:

1. **Use Spin Command Trigger first** (already working)
   - Good for computation-only workloads
   - No HTTP interface needed

2. **Document the full HTTP requirements**
   - This approach is complex but feasible
   - Requires WIT deps, adapters, and resource management

3. **Contribute upstream**
   - The component model for C is still evolving
   - Tooling may improve over time

## References

- [WASI HTTP Spec](https://github.com/WebAssembly/wasi-http)
- [Component Model](https://component-model.bytecodealliance.org/)
- [wit-bindgen CLI](https://github.com/bytecodealliance/wit-bindgen)
- [wasm-tools](https://github.com/bytecodealliance/wasm-tools)
- [Spin HTTP Trigger Docs](https://spinframework.dev/v2/http-trigger)
- [Sample C Component](https://docs.wasmtime.dev/api/wasmtime/component/macro.bindgen.html)

## Next Steps

To proceed with full HTTP implementation:

1. ⬜ Set up WIT dependencies in `wit/deps/wasi-http/`
2. ⬜ Generate HTTP bindings with `wit-bindgen c`
3. ⬜ Implement `handle_incoming_request` function
4. ⬜ Use component adapters to create AtomVM-http.wasm
5. ⬜ Test with wasmtime and Spin
6. ⬜ Document integration with AtomVM's request/response handling

Or, document current limitations and proceed with command trigger for production use cases.