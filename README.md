# MemoryLeakDetector

[![CI](https://github.com/ArturToJa/MemoryLeakDetector/actions/workflows/ci.yml/badge.svg)](https://github.com/ArturToJa/MemoryLeakDetector/actions/workflows/ci.yml)

A C++20 memory leak detector built around intercepting global `operator new`/`operator delete`, with symbolized stack traces for every allocation still outstanding at program exit.

```
========== LEAK REPORT ==========
[LEAK] 0000020088B1CE80 | 4 bytes
  Stack trace:
operator new - NewDelete.cpp:72
main - TestApp.cpp:29
...
Total leaked: 268 bytes
```

## How it works

- **[NewDelete.cpp](MemoryAnalyzerCore/NewDelete.cpp)** overrides every `operator new`/`delete` variant (plain, array, nothrow, aligned) and routes each call through `PlatformMemory` for the actual allocation, capturing a stack trace and pushing an `AllocationEvent` onto a queue.
- **[EventQueue](MemoryAnalyzerCore/EventQueue.h)** → **[TrackerThread](MemoryAnalyzerCore/TrackerThread.h)** hand events off to a dedicated background thread, so tracking never sits on the hot allocation path.
- **[Tracker](MemoryAnalyzerCore/Tracker.h)** keeps a map of everything currently allocated; at shutdown, `reportLeaks()` prints everything still outstanding with a symbolized stack trace.
- **[PlatformStackTrace](MemoryAnalyzerCore/Platform/StackTrace/PlatformStackTrace.h)** / **[SymbolResolver](MemoryAnalyzerCore/Platform/StackTrace/SymbolResolver.h)** do the platform-specific stack walk and symbolization (Windows: `dbghelp`; Linux: `backtrace`/`dladdr` + `addr2line`; macOS: `backtrace`/`dladdr` + `atos`).
- **[InterceptorGuard](MemoryAnalyzerCore/InterceptorGuard.h)** / **`TrackingGuard`** are thread-local reentrancy guards so the tracker's own allocations (map nodes, `std::cout`, `addr2line`'s pipe buffer, ...) don't recursively re-trigger tracking.
- **[Runtime](MemoryAnalyzerCore/Runtime.h)** auto-starts everything via a static initializer and registers an `atexit` handler to print the leak report - linking `MemoryAnalyzerCore` into a project is meant to be enough, no code changes required.

## What this can and can't catch

This is a deliberately scoped, link-time tool, not a full memory debugger:

- Only C++ allocation (`operator new`/`delete` and their variants) is intercepted - raw `malloc`/`free`/`calloc`/`realloc` and anything allocated by other libraries outside `operator new` is out of reach here, and deliberately so: `operator new`/`delete` are portably replaceable *by the C++ standard itself*, which is exactly why a plain static library can override them uniformly across MSVC/GCC/Clang with no platform-specific code. `malloc` has no equivalent standard replacement mechanism - overriding it always means adopting some form of load-time interposition instead (glibc's undocumented `__libc_malloc` + `dlsym(RTLD_NEXT, ...)` via `LD_PRELOAD` on Linux, IAT patching on Windows, `DYLD_INTERPOSE` via `DYLD_INSERT_LIBRARIES` on macOS). That's a fundamentally different mechanism than "link a static lib and it works," and it's exactly the mechanism the injection-based `MemoryAnalyzer` DLL is for - `malloc` support belongs there, not here. (One thing worth carrying over when that work starts: `operator new` on Linux/libstdc++ typically calls `malloc` internally, so hooking both in the same process would double-count a single `new` as two events unless something suppresses the inner one - the `InterceptorGuard`/`TrackingGuard` reentrancy guards already in Core happen to solve exactly this, being thread-local and scoped to the whole call chain rather than one function, so that piece likely ports over as-is.) Lower-level allocation (`mmap`/`VirtualAlloc`, a custom arena/pool allocator, a `std::pmr` resource backed by its own buffer, an embedded interpreter's own allocator) is out of reach for any generic tool unless it happens to route through `operator new`/`malloc` internally - not something either half of this project can chase in general.
- Very early allocations (from another translation unit's global constructor that happens to run before this library's own static initializer) are still tracked correctly: `NewDelete.cpp`'s `trackAllocation`/`trackDeallocation` self-initialize on first use rather than relying on winning the static-init-order race, which the `#pragma init_seg(lib)` / `__attribute__((init_priority))` hints on `RuntimeInitializer` (see [Runtime.cpp](MemoryAnalyzerCore/Runtime.cpp)) can narrow but never guarantee - confirmed in practice on Darwin, where neither hint applies at all. The corresponding shutdown-side hazard - a *different* translation unit's global deallocating during its own teardown, after this library's own tracking machinery has already been torn down - is handled the same way, by cleanly declining to track rather than touching already-destroyed state (see `Runtime::isShutdownComplete()`).
- A global/static object that's intentionally alive for the whole process lifetime (never explicitly freed, but not actually a bug) will show up in the report as a leak - this tool doesn't yet distinguish "definitely lost" from "still reachable" the way tools like LeakSanitizer do.
- Only single-module coverage: allocations inside a different module (a DLL/shared library that doesn't itself link `MemoryAnalyzerCore`) are invisible to it. That's the gap the injection-based `MemoryAnalyzer` DLL (in progress, see below) is meant to close.

## Building

Requires CMake 3.21+ and a C++20 compiler.

```bash
cmake --preset x64-Debug           # Windows (MSVC)
cmake --preset x64-clang-cl-Debug  # Windows (clang-cl)
cmake --preset linux-debug         # Linux (default toolchain, typically GCC)
cmake --preset linux-clang-debug   # Linux (Clang)
cmake --preset macos-debug         # macOS (Clang)
cmake --build --preset <preset-name>
ctest --preset <preset-name> --output-on-failure
```

CI ([`.github/workflows/ci.yml`](.github/workflows/ci.yml)) runs the full test suite on every push across all five combinations above (MSVC, clang-cl, GCC, Clang, Apple Clang).

## Project layout

| Path | Purpose |
|---|---|
| `MemoryAnalyzerCore/` | The static library described above - link it into any target. |
| `TestApp/` | A manual smoke test exercising every allocation form (plain/array/nothrow/aligned, matched frees, deliberate leaks). |
| `tests/` | Automated tests: `Tracker` unit tests, and end-to-end lifetime scenarios (global objects, function-local statics, cross-thread allocation) verified against the real interception pipeline via CTest. |
| `MemoryAnalyzer/` | **Not yet implemented.** A DLL meant to be injected into an already-built executable (no source access, no relinking) to run the same leak detection - the counterpart to `MemoryAnalyzerCore`'s link-time approach. |

## Status

- ✅ Core interception, tracking, and symbolized reporting - Windows, Linux, and macOS, tested end-to-end via CI on real hardware for each (MSVC, clang-cl, GCC, Clang, Apple Clang).
- ✅ Automated tests covering allocation forms, object lifetimes, and multi-threaded allocation.
- ⏳ Injection-based `MemoryAnalyzer` DLL - not started; current contents are an early prototype predating the current design and aren't wired into the build.
