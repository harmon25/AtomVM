# Elixir WASI Examples

Elixir examples for AtomVM running on the WASI platform.

## Overview

These examples demonstrate running Elixir code on AtomVM via WebAssembly System Interface (WASI). Elixir runs on WASI by leveraging:

- **exavmlib** - Elixir standard library modules (Enum, IO, Map, etc.)
- **estdlib** - Erlang standard library (lists, maps, string, etc.)
- **eavmlib** - AtomVM extended library for console I/O

## Prerequisites

- **wasi-sdk** (>= 30) - Clang/LLVM toolchain for wasm32-wasip2
- **wasmtime** - WASI runtime
- **Erlang/OTP** (>= 26) - for `erlc` compiler
- **Elixir** (>= 1.14) - for `elixirc` compiler
- **AtomVM built for WASI** - see main WASI platform README

## Building AtomVM for WASI

From the repository root:

```bash
# Build AtomVM.wasm
export WASI_SDK_PATH=/path/to/wasi-sdk
cmake -S src/platforms/wasi -B build-wasi \
  -DCMAKE_TOOLCHAIN_FILE=cmake/wasi-sdk.cmake
cmake --build build-wasi

# Build PackBEAM and libraries (native host build)
cmake -S . -B build
cmake --build build --target PackBEAM
cmake --build build --target estdlib
cmake --build build --target eavmlib
cmake --build build --target exavmlib
```

## Files

| File | Description |
|------|-------------|
| `simple_demo.ex` | Hello world + basic Elixir features (lists, maps, recursion) |
| `pure_erlang_demo.ex` | Demo using only Erlang stdlib functions |
| `spin_handler.ex` | HTTP request handler for Fermyon Spin |
| `build_and_run.sh` | Build script for Spin HTTP handler |

## Running with Wasmtime

### Simple Demo

```bash
cd examples/elixir/wasi

# Compile Elixir to BEAM
elixirc --no-docs --no-debug-info -o /tmp simple_demo.ex

# Package with all required libraries
../../../build/tools/packbeam/PackBEAM simple_demo.avm \
  /tmp/Elixir.SimpleDemo.beam \
  ../../../build/libs/exavmlib/lib/exavmlib.avm \
  ../../../build/libs/estdlib/src/estdlib.avm \
  ../../../build/libs/eavmlib/src/eavmlib.avm

# Run with wasmtime
wasmtime --dir=/tmp ../../../build-wasi/AtomVM.wasm -- simple_demo.avm
```

Expected output:
```
Hello from Elixir on WASI!

Numbers: [1, 2, 3, 4, 5]
Doubled: [2, 4, 6, 8, 10]
Sum: 15

User: %{name: "Alice", age: 30}
Name: Alice, Age: 30

5! = 120

Sum of squares of evens: 20

Demo complete!
Return value: ok
```

## Supported Features

### ✅ What Works

- **IO functions**: `IO.puts/1`, `IO.inspect/1`, string interpolation
- **Enum functions**: `Enum.map/2`, `Enum.filter/2`, `Enum.count/1`
- **Map access**: `map.key` syntax, `Map.get/2`
- **Pattern matching**: Maps, lists, tuples
- **Recursion**: Tail-call optimized functions
- **Private functions**: `defp` definitions
- **Pipe operator**: `|>` (with limitations, see below)

### ⚠️ Important Limitations

1. **Cannot pipe into Erlang functions with wrong argument order**
   
   The pipe operator `|>` puts the value as the **first argument**, but many Erlang functions expect it as a different position:
   
   ```elixir
   # ❌ WRONG: Becomes :lists.foldl(list, fn, 0)
   list |> :lists.foldl(fn x, acc -> x + acc end, 0)
   
   # ✅ CORRECT: Call foldl directly
   :lists.foldl(fn x, acc -> x + acc end, 0, list)
   
   # ✅ CORRECT: Works with Elixir functions
   list |> Enum.map(fn x -> x * 2 end)
   ```

2. **Use Erlang stdlib for some operations**
   
   Since `exavmlib` is a subset of Elixir stdlib, use Erlang equivalents:
   
   | Elixir (not available) | Erlang (use instead) |
   |------------------------|----------------------|
   | `Enum.sum/1` | `:lists.foldl(fn x, acc -> x + acc end, 0, list)` |
   | `String.upcase/1` | `:string.to_upper/1` |
   | `String.length/1` | `:string.length/1` |
   | `Enum.reduce/3` | `:lists.foldl/3` |

3. **Elixir stdlib not fully available**
   
   Only modules in `exavmlib` are available. Common modules like `String`, `Date`, `Regex` are not included. Use Erlang equivalents from `estdlib`.

### Available Modules in exavmlib

Run this to see all available Elixir modules:

```bash
build/tools/packbeam/PackBEAM -l build/libs/exavmlib/lib/exavmlib.avm
```

Key modules:
- `Enum` - map, filter, count, at, slice, etc.
- `IO` - puts, inspect
- `Map` - get, put, delete
- `List` - first, last, flatten
- `Kernel` - basic functions
- `String.Chars` - to_string protocol
- Various error types: `ArgumentError`, `RuntimeError`, etc.

## Writing Elixir for WASI

### Good Example

```elixir
defmodule MyApp do
  def start do
    # Use Elixir functions when available
    numbers = [1, 2, 3, 4, 5]
    doubled = Enum.map(numbers, fn x -> x * 2 end)
    IO.puts("Doubled: #{inspect(doubled)}")
    
    # Use Erlang stdlib for missing functions
    sum = :lists.foldl(fn x, acc -> x + acc end, 0, numbers)
    IO.puts("Sum: #{sum}")
    
    # Pattern matching works
    user = %{name: "Alice", age: 30}
    IO.puts("Name: #{user.name}")
    
    :ok
  end
end
```

### Tips

1. **Check function availability first** - If you get "cannot be resolved" errors, the function isn't in exavmlib
2. **Use `:module.function` syntax** for Erlang stdlib
3. **Test incrementally** - Build and test small pieces before the full application
4. **Check return values** - The VM prints `Return value: ok` or `Return value: error` at the end

## Running with Fermyon Spin

See the [Erlang WASI examples](../erlang/wasi/) for Spin setup instructions. The Elixir build script automates the process:

```bash
cd examples/elixir/wasi
./build_and_run.sh
```

## Troubleshooting

### "Warning: function X cannot be resolved"

The function doesn't exist in the available libraries. Check:
- Is it in exavmlib? Use an Erlang equivalent
- Is the module name correct? (e.g., `:lists` not `Lists`)
- Are you piping into an Erlang function with wrong argument order?

### "Return value: error"

Something crashed during execution. Common causes:
- Undefined function
- Pattern match failure
- Calling a function with wrong arguments

Add `IO.puts/1` statements to trace where it fails.

### Console output not showing

Try using `:io.format/2` instead of `IO.puts/1`:

```elixir
:io.format("Value: ~p~n", [value])
```

## See Also

- [WASI Platform README](../../../src/platforms/wasi/README.md) - Full WASI documentation
- [Erlang WASI Examples](../erlang/wasi/) - Erlang versions of these examples
- [AtomVM Documentation](https://www.atomvm.net/) - Official AtomVM docs
