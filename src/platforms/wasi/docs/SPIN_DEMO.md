# AtomVM on Fermyon Spin - Proof of Concept

## Summary

✅ **SUCCESS**: AtomVM successfully runs Erlang code on Fermyon Spin using the WASI platform!

## What Was Demonstrated

1. **Installed Spin CLI** (v3.5.1) with the command trigger plugin
2. **Modified AtomVM** to auto-detect embedded AVM files (for Spin compatibility)
3. **Created Spin application** with command trigger
4. **Ran Erlang code** successfully without CLI arguments

## Key Changes Made

### 1. Modified `src/platforms/wasi/main.c`

Added support for default AVM file paths when no CLI arguments are provided:

```c
// For Spin compatibility: if no arguments provided, try default paths
const char *default_avm_paths[] = {"/test.avm", "/app.avm", "./app.avm", NULL};
if (first_file_arg >= argc) {
    // Try default paths (useful for Spin deployment)
    const char **path = default_avm_paths;
    while (*path != NULL) {
        if (access(*path, F_OK) == 0) {
            argv[argc++] = (char *)*path;
            first_file_arg = argc - 1;
            break;
        }
        path++;
    }
    ...
}
```

This allows AtomVM to work with Spin's command trigger which doesn't pass CLI arguments.

### 2. Created `spin.toml` Manifest

```toml
spin_manifest_version = 2

[application]
name = "erlang-spin-demo"
version = "0.1.0"

[[trigger.command]]
component = "erlang-app"

[component.erlang-app]
source = "AtomVM.wasm"
files = [{ source = "myapp.avm", destination = "/test.avm" }]
```

## Working Example

### Simple Erlang Module (works)
```erlang
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

**Result**: ✅ Returns `ok`, Spin shows `Return value: ok`

### With IO (limited)
```erlang
-module(spin_handler).
-export([start/0]).

start() ->
    io:format("Hello from Erlang on Spin!~n"),
    ok.
```

**Result**: ❌ `RUN_RESULT_NOT_OK` - `io:format` requires stdout which isn't available in Spin's command trigger

## Limitations Found

1. **No stdin/stdout**: Spin's command trigger doesn't provide TTY access, so `io:format` fails
2. **No CLI arguments**: Command trigger executes the binary but doesn't pass arguments (worked around with default paths)
3. **No HTTP trigger support**: Would require implementing `wasi:http/incoming-handler` interface

## To Run the Demo

```bash
# Build AtomVM for WASI (already done)
cd /home/harmon/Dev/AtomVM/build-wasi

# Create Spin app
cd /tmp/atomvm-test
cat > spin.toml << 'EOF'
spin_manifest_version = 2

[application]
name = "erlang-spin-demo"
version = "0.1.0"

[[trigger.command]]
component = "erlang-app"

[component.erlang-app]
source = "AtomVM.wasm"
files = [{ source = "spin_simple.avm", destination = "/test.avm" }]
EOF

# Run with Spin
spin up
```

## Files Location

All test files are in: `/tmp/atomvm-test/`
- `AtomVM.wasm` - The modified AtomVM binary
- `spin_simple.avm` - Working Erlang demo
- `spin_handler.avm` - IO demo (limited)
- `spin.toml` - Spin manifest

## Conclusion

AtomVM CAN run on Fermyon Spin, but with limitations:
- ✅ Pure computation works (math, pattern matching, recursion)
- ❌ Console I/O doesn't work in command trigger mode
- ❌ Raw sockets don't work (Spin doesn't expose them)
- ✅ Could work with HTTP trigger if we implement `wasi:http/incoming-handler`

For full Spin integration, would need:
1. HTTP trigger support (implement WASI HTTP interface)
2. Use Spin's `wasi:http` for outbound requests instead of raw sockets
3. Use Spin's KV store or SQLite instead of file I/O (optional)