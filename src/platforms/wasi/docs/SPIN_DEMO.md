# AtomVM on Fermyon Spin - Status & Guide

## Status Overview

| Feature | Status | Notes |
|---------|--------|-------|
| **Command Trigger** | ✅ Working | Pure computation works perfectly |
| **HTTP Trigger** | ⚠️ Component Model | Requires component model implementation |
| **Redis Trigger** | ❌ Not implemented | Requires specific adapters |
| **Configurable Triggers** | ❌ Not implemented | Requires component model |

---

## ✅ Command Trigger (Working)

The command trigger **already works** and is the easiest way to run AtomVM on Spin.

### Demonstration

1. **Install Spin CLI** (v3.5.1+):
```bash
curl -fsSL https://spinframework.dev/downloads/install.sh | bash

# Install command trigger plugin
spin plugins install trigger-command
```

2. **Build AtomVM**:
```bash
cd /home/harmon/Dev/AtomVM
export WASI_SDK_PATH=/home/harmon/Libs/wasi-sdk-30
cmake -S src/platforms/wasi -B build-wasi -DCMAKE_TOOLCHAIN_FILE=cmake/wasi-sdk.cmake
cmake --build build-wasi
```

3. **Create a simple Erlang module**:
```erlang
%% spin_simple.erl
-module(spin_simple).
-export([start/0]).

start() ->
    X = 2 + 2,
    Y = X * 10,
    case Y of
        40 -> ok;
        _ -> error
    end.
```

4. **Compile and package**:
```bash
erlc spin_simple.erl
build/tools/packbeam/PackBEAM spin_simple.avm spin_simple.beam \
  build/libs/estdlib/src/estdlib.avm \
  build/libs/eavmlib/src/eavmlib.avm
```

5. **Create Spin application**:
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

6. **Run**:
```bash
spin up
# Output: Return value: ok
```

### Key Features

- ✅ Runs Erlang/Elixir code successfully
- ✅ Support for process spawning
- ✅ Pattern matching, recursion, higher-order functions
- ✅ List and map operations
- ✅ File I/O with `--dir=` flag
- ✅ Socket networking on localhost (127.0.0.1)

### Limitations

- ❌ No stdin/stdout - `io:format` doesn't produce visible output
- ❌ No CLI arguments - handled by auto-detection of `/test.avm`, `/app.avm`, `./app.avm`
- ❌ No raw socket access - Spin uses different networking model

---

## ⚠️ HTTP Trigger (Component Model Required)

The HTTP trigger requires using the WASI **component model**, which adds complexity beyond the core module approach.

### What is the Component Model?

The component model is an extension to WebAssembly that:
- Defines interfaces using WIT (WebAssembly Interface Type)
- Provides better type safety and interoperability
- Enables rich host-guest interactions (like HTTP)

### HTTP Trigger Implementation Status

**Current State**: Research and prototyping phase.

**Components Needed**:
1. ✅ WIT bindings generation (`wit-bindgen c`)
2. ⚠️ Component adapter layer
3. ⚠️ `wasi:http/incoming-handler` implementation
4. ❌ Full integration with AtomVM

### Implementation Requirements

To enable HTTP trigger support, these steps are needed:

#### 1. WIT Interface Definition

```wit
package atomvm:http

world app {
  export wasi:http/incoming-handler
}
```

#### 2. Bindings Generation

```bash
# Requires proper WIT dependency structure
wit-bindgen c --out-dir . app.wit
```

#### 3. Component Building

```bash
# Component adapter converts core module to component
wasm-tools component new \
    --adapt=/path/to/wasi_snapshot_preview1.reactor.wasm \
    src/platforms/wasi/AtomVM.wasm \
    -o AtomVM-http.wasm
```

#### 4. HTTP Handler Implementation

```c
void handle_incoming_request(
    incoming_request_t *request,
    response_outparam_t response_out
) {
    // Parse HTTP request
    method_t method = request->method();
    path_with_query_t path = request->path_with_query();
    headers_t headers = request->headers();
    body_t body = request->consume();

    // Call into AtomVM (pattern)
    // Send response...
}
```

### Available Source Files

We've created experimental source files in `src/platforms/wasi/http/`:
- `wit/app.wit` - WIT interface definition
- `host.h`, `host.c` - Generated bindings
- `host_impl.c` - Implementation skeleton
- `build-component.sh` - Build script
- `README.md` - Detailed technical documentation

### Work Remaining

| Task | Status | Effort |
|------|--------|--------|
| WIT dependency setup | 🔧 In progress | Medium |
| HTTP handler implementation | 🚧 Not started | High |
| AtomVM integration | 🚧 Not started | High |
| Testing with Spin | 🚧 Not started | Medium |

### Alternative Approaches

If time-constrained or complexity is a concern:

1. **Use Command Trigger** (works now!)
   - Simple, proven approach
   - Good for computation-only workloads

2. **Wait for Tooling Improvements**
   - Component model tooling for C is evolving
   - Future versions may simplify the process

3. **Contribute Upstream**
   - The `wasip2` ecosystem is young
   - Documentation and examples will improve

---

## Testing Spin Applications

### Command Trigger

```bash
# In directory with spin.toml
spin up

# View logs
spin logs
```

### HTTP Trigger (when implemented)

```bash
# Start Spin
spin up

# Test HTTP endpoint
curl http://localhost:3000/

# View logs
spin logs
```

---

## Spin Configuration Reference

### Command Trigger (`[[trigger.command]]`)

```toml
[[trigger.command]]
component = "component-name"
```

- ✅ Supports `files` directive to mount AVM files
- ✅ Works with any WASM component
- ❌ No HTTP interface
- ❌ Limited I/O (no TTY)

### HTTP Trigger (`[[trigger.http]]`)

```toml
[[trigger.http]]
route = "/..."
component = "component-name"
```

- ✅ Full HTTP request/response
- ✅ Access to headers, method, body
- ❌ Requires component model
- ❌ Complex setup

---

## Example Project Structure

```
atomvm-spin-demo/
├── spin.toml              # Spin manifest
├── AtomVM.wasm            # Command trigger: core module
├── AtomVM-http.wasm       # HTTP trigger: component (TBD)
├── spin_simple.avm        # Test application
└── README.md              # Project documentation
```

---

## Resources

- [WASI Platform Documentation](../WASI.md)
- [Component Model Guide](http/README.md)
- [WASI HTTP Spec](https://github.com/WebAssembly/wasi-http)
- [Spin Documentation](https://spinframework.dev/)
- [Component Model](https://component-model.bytecodealliance.org/)

---

## Conclusion

**For production use today**: Command trigger is ready and working.

For HTTP trigger support: Requires additional component model implementation - see `src/platforms/wasi/http/README.md` for details and ongoing work.