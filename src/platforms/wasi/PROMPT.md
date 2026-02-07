
# Task: Create a WASI Platform Target for AtomVM (Spin-compatible)

You are working inside the **AtomVM repository**.

Your goal is to create a **new WASI platform target** that can compile AtomVM to a `wasm32-wasi` binary capable of running simple Elixir programs using stdin/stdout and environment variables.

This target is intended to run inside **Spin/Fermyon** and other WASI runtimes.

IMPORTANT:
This is an MVP implementation. Do NOT attempt to implement full networking, SMP, or distributed features.

We are building the smallest possible functional WASI runtime.

---

# High-Level Goal

Create a new platform:

```
src/platforms/wasi/
```

based primarily on:

```
src/platforms/generic_unix/
```

NOT emscripten.

The result should compile AtomVM into:

```
atomvm.wasm
```

using:

```
clang --target=wasm32-wasi
```

and run inside:

```
wasmtime atomvm.wasm
```

---

# Scope Constraints (MVP)

Supported:

* stdin input
* stdout output
* environment variables
* monotonic clock
* timers/sleep
* single-threaded execution
* one scheduler

Not supported:

* distributed Erlang
* sockets/networking
* SMP / multiple schedulers running in parallel
* NIFs requiring OS features
* epoll/select
* pthreads

Keep implementation minimal.
