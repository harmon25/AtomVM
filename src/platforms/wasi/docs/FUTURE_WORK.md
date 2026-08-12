# AtomVM WASI HTTP — Future Work & Optimization

## Performance Baseline

Initial benchmarks (single connection, 5s duration, `oha` load tester) comparing
the AtomVM/Elixir HTTP handler to an equivalent Rust handler on Fermyon Spin:

| Route      | AtomVM Req/s | Rust Req/s | Ratio  | AtomVM Avg | Rust Avg  |
|------------|-------------:|-----------:|-------:|-----------:|----------:|
| `/hello`   |          629 |      4,426 | ~7x    |    1.58 ms |   0.22 ms |
| `/json`    |          636 |      4,227 | ~6.6x  |    1.57 ms |   0.23 ms |
| `/compute` |          537 |      4,215 | ~7.8x  |    1.86 ms |   0.23 ms |

At 10 concurrent connections, AtomVM scales to ~10k req/s and the ratio
narrows to ~4-6x, since Spin parallelizes across instances.

The `/compute` route (fib(10), factorial(12), list operations) adds only ~0.3ms
over the `/hello` baseline, showing that BEAM interpretation speed is not the
bottleneck — per-request VM initialization is.

## Root Cause

Spin creates a fresh WASM instance for each HTTP request. Every request pays:

1. **WASM instantiation** (~0.1-0.2ms, Spin-side)
2. **`atomvm_http_init` constructor** (~0.5-1ms):
   - `globalcontext_new()` — allocates the VM's global state
   - `sys_open_avm_from_file()` — reads app.avm from the WASI filesystem
   - `avmpack_find_section_by_flag()` — scans for the startup module
   - `module_new_from_iff_binary()` — parses BEAM bytecode
   - `globalcontext_insert_module()` — registers the module
3. **Handler dispatch** (~0.2-0.3ms):
   - `context_new()` + heap allocation
   - Request map construction
   - `globalcontext_get_module()` — module lookup from AVM pack
   - `context_execute_loop()` — enters the scheduler to run `handle/1`
4. **Response marshaling** (~0.1ms)

The Rust handler pays only for step 1 plus a trivial function call. Steps 2-3
are the optimization target.

## Optimization Opportunities

### 1. Pre-Initialization Snapshots (Highest Impact)

**Expected improvement: eliminate ~1ms per request (~50-60% of overhead)**

Spin and wasmtime support pre-initialized WASM snapshots via `wasi-virt` or
Spin's built-in pre-initialization. The idea: run the `__attribute__((constructor))`
once at build time, snapshot the WASM linear memory (with GlobalContext, AVM
pack, and startup module already loaded), and start each request from the
post-init state.

This would completely eliminate the per-request file I/O, AVM parsing, and
module loading — the most expensive steps.

#### Implementation Status

**Current Blocker**: The AtomVM HTTP component is built as a WebAssembly Component Model
binary (not a core WASM module). Tools like [Wizer](https://github.com/bytecodealliance/wizer)
require core WASM modules. Additionally, the current initialization (`atomvm_http_init`)
uses WASI filesystem APIs (`access()`, `sys_open_avm_from_file()`) to load the AVM file,
which cannot be snapshotted by current pre-init tools.

#### Solution Path

**Phase 1: Embed AVM File (Completed)**
- Modify `main_http.c` to support loading AVM from embedded data section
- Create CMake target that embeds AVM file into WASM binary using `wasm-tools`
- Remove filesystem dependency during initialization

**Phase 2: Create Pre-Init Build Target**
```cmake
# Add to src/platforms/wasi/CMakeLists.txt
add_custom_target(AtomVM_http_preinit
  COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/http/embed_and_preinit.sh
    $<TARGET_FILE:AtomVM_http>
    ${CMAKE_CURRENT_BINARY_DIR}/AtomVM_http_preinit.wasm
  DEPENDS AtomVM_http
)
```

**Phase 3: Wizer Integration**
```bash
# Build process
1. Build AtomVM_http as core module (not component) for pre-init
2. Embed AVM file into data section
3. Run wizer to snapshot initialized state:
   wizer --allow-wasi -o AtomVM_http_preinit.wasm AtomVM_http_embedded.wasm
4. Wrap snapshotted core module back into component
```

#### Build Script

A prototype build script is available at:
`src/platforms/wasi/http/embed_and_preinit.sh`

Usage:
```bash
cd build-wasi
../src/platforms/wasi/http/embed_and_preinit.sh app.avm
```

#### References
- [Spin Component Model](https://spinframework.dev/component-model/)
- [wasi-virt](https://github.com/bytecodealliance/wasi-virt)
- [wasmtime pre-initialization](https://docs.wasmtime.dev/api/wasmtime/component/struct.Linker.html)
- [Wizer](https://github.com/bytecodealliance/wizer) - WebAssembly pre-initializer

### 2. Cache Handler Module Pointer at Init (Medium Impact)

**Expected improvement: ~0.1-0.2ms per request**

Currently, every request calls `globalcontext_get_module()` which searches the
AVM pack's section list. Since the handler module is always the same, cache
the `Module *` pointer and its atom index during `atomvm_http_init()`.

```c
// In main_http.c, after loading the startup module:
static Module *g_handler_module = NULL;
g_handler_module = startup_module;

// In wasi_http_handler.c, skip the lookup:
Module *handler_mod = g_handler_module;
```

### 3. Pre-Create Atom Keys at Init (Low-Medium Impact)

**Expected improvement: ~0.05-0.1ms per request**

Every request calls `globalcontext_make_atom()` for the same strings: `"method"`,
`"path"`, `"headers"`, `"body"`, `"status"`, `"authority"`. These atom lookups
involve string hashing and hash table operations. Create them once during init
and cache the `term` values in globals.

```c
static term g_method_atom, g_path_atom, g_headers_atom, g_body_atom;
// Set during atomvm_http_init()
```

### 4. Lighter-Weight Execution Path (Medium Impact)

**Expected improvement: ~0.1-0.2ms per request**

`context_execute_loop()` enters the full AtomVM scheduler loop, which is
designed for multi-process concurrent Erlang (ready queues, waiting queues,
scheduler fairness). For a synchronous single-function call, a direct
"call this exported function and return" path would be leaner.

This would involve:
- A new `context_execute_function()` that directly dispatches to the module's
  export without entering the scheduler
- Skipping process lifecycle management (spawn, link, monitor)
- Returning the result directly from `x[0]` after the function returns

### 5. Trim AVM Pack Size (Low Impact)

**Expected improvement: faster AVM scanning, smaller WASM memory footprint**

The app.avm currently includes the full `estdlib.avm` and `eavmlib.avm`
(~100+ modules). Even though modules are loaded on-demand, the AVM pack
scanning has overhead proportional to pack size. A trimmed pack containing
only the modules actually used by the handler would reduce both scan time
and memory usage.

A `packbeam --tree-shake` or `--used-only` mode could automate this.

### 6. Module Pre-Loading at Init (Low Impact)

**Expected improvement: avoid lazy-load overhead on first call to each module**

Currently, modules like `spin_http` are loaded lazily on first use via
`globalcontext_get_module()`. Pre-loading all NIF stub modules during
`atomvm_http_init()` would move this cost out of the request path.

## Comparison Notes

- **BEAM (Erlang/OTP's VM)** would be significantly slower than AtomVM in this
  context. BEAM has a much heavier initialization (boot server, code server,
  application controller, etc.) and is not designed for sub-millisecond cold
  starts. AtomVM is already the lightest possible BEAM implementation.

- The fundamental architectural tension is between Spin's per-request
  instantiation model and a VM that requires initialization. Pre-init
  snapshots (optimization #1) are the right architectural fix — they let
  you pay the init cost once at build time rather than per request.

- At ~630 req/s per instance (single connection), AtomVM is already usable
  for many real-world workloads. Spin's horizontal scaling (multiple
  instances) brings aggregate throughput much higher.
