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
- **[PlatformStackTrace](MemoryAnalyzerCore/Platform/StackTrace/PlatformStackTrace.h)** / **[SymbolResolver](MemoryAnalyzerCore/Platform/StackTrace/SymbolResolver.h)** do the platform-specific stack walk and symbolization (Windows: `dbghelp`; Linux: `backtrace`/`dladdr` + `addr2line`).
- **[InterceptorGuard](MemoryAnalyzerCore/InterceptorGuard.h)** / **`TrackingGuard`** are thread-local reentrancy guards so the tracker's own allocations (map nodes, `std::cout`, `addr2line`'s pipe buffer, ...) don't recursively re-trigger tracking.
- **[Runtime](MemoryAnalyzerCore/Runtime.h)** auto-starts everything via a static initializer and registers an `atexit` handler to print the leak report - linking `MemoryAnalyzerCore` into a project is meant to be enough, no code changes required.

## What this can and can't catch

This is a deliberately scoped, link-time tool, not a full memory debugger:

- Only C++ allocation (`operator new`/`delete` and their variants) is intercepted - raw `malloc`/`free` and anything allocated by other libraries outside `operator new` is out of reach here by design.
- Very early allocations (from another translation unit's global constructor that happens to run before this library's own static initializer) can go untracked - narrowed via `#pragma init_seg(lib)` / `__attribute__((init_priority))`, but not eliminated; see the comment on `RuntimeInitializer` in [Runtime.cpp](MemoryAnalyzerCore/Runtime.cpp).
- A global/static object that's intentionally alive for the whole process lifetime (never explicitly freed, but not actually a bug) will show up in the report as a leak - this tool doesn't yet distinguish "definitely lost" from "still reachable" the way tools like LeakSanitizer do.
- Only single-module coverage: allocations inside a different module (a DLL/shared library that doesn't itself link `MemoryAnalyzerCore`) are invisible to it. That's the gap the injection-based `MemoryAnalyzer` DLL (in progress, see below) is meant to close.

## Building

Requires CMake 3.21+ and a C++20 compiler.

```bash
cmake --preset x64-Debug      # Windows (MSVC)
cmake --preset linux-debug    # Linux (GCC/Clang)
cmake --build --preset <preset-name>
ctest --preset <preset-name> --output-on-failure
```

CI ([`.github/workflows/ci.yml`](.github/workflows/ci.yml)) builds and runs the full test suite on Windows/MSVC on every push; the Linux preset builds and runs on Ubuntu/GCC alongside it.

## Project layout

| Path | Purpose |
|---|---|
| `MemoryAnalyzerCore/` | The static library described above - link it into any target. |
| `TestApp/` | A manual smoke test exercising every allocation form (plain/array/nothrow/aligned, matched frees, deliberate leaks). |
| `tests/` | Automated tests: `Tracker` unit tests, and end-to-end lifetime scenarios (global objects, function-local statics, cross-thread allocation) verified against the real interception pipeline via CTest. |
| `MemoryAnalyzer/` | **Not yet implemented.** A DLL meant to be injected into an already-built executable (no source access, no relinking) to run the same leak detection - the counterpart to `MemoryAnalyzerCore`'s link-time approach. |

## Status

- ✅ Core interception, tracking, and symbolized reporting - Windows and Linux, tested end-to-end.
- ✅ Automated tests covering allocation forms, object lifetimes, and multi-threaded allocation.
- ⏳ macOS support - not started.
- ⏳ Injection-based `MemoryAnalyzer` DLL - not started; current contents are an early prototype predating the current design and aren't wired into the build.
