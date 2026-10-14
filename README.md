# MemoryLeakDetector

[![CI](https://github.com/ArturToJa/MemoryLeakDetector/actions/workflows/ci.yml/badge.svg)](https://github.com/ArturToJa/MemoryLeakDetector/actions/workflows/ci.yml)

A C++20 memory leak detector with symbolized stack traces for every allocation still outstanding at program exit. It comes in two forms sharing one engine:

- **`MemoryAnalyzerCore`** - a static library you link in; intercepts global `operator new`/`operator delete`. Windows, Linux, macOS.
- **`MemoryAnalyzer`** - a library injected into an already-built executable (no source access, no relinking); intercepts `malloc`/`free`/`calloc`/`realloc` as well as `operator new`/`delete`. **Linux** (via `LD_PRELOAD`) and **Windows** (launcher + DLL injection + IAT patching) so far.

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

### Shared engine (`MemoryAnalyzerEngine`)

- **[EventQueue](MemoryAnalyzerEngine/EventQueue.h)** → **[TrackerThread](MemoryAnalyzerEngine/TrackerThread.h)** hand `AllocationEvent`s ([Events.h](MemoryAnalyzerEngine/Events.h)) off to a dedicated background thread, so tracking never sits on the hot allocation path.
- **[Tracker](MemoryAnalyzerEngine/Tracker.h)** keeps a map of everything currently allocated; at shutdown, `reportLeaks()` prints everything still outstanding with a symbolized stack trace.
- **[PlatformMemory](MemoryAnalyzerEngine/PlatformMemory.h)** wraps the platform's raw allocation calls.
- **[PlatformStackTrace](MemoryAnalyzerEngine/Platform/StackTrace/PlatformStackTrace.h)** / **[SymbolResolver](MemoryAnalyzerEngine/Platform/StackTrace/SymbolResolver.h)** do the platform-specific stack walk and symbolization (Windows: `dbghelp`; Linux: `backtrace`/`dladdr` + `addr2line`; macOS: `backtrace`/`dladdr` + `atos`).
- **[InterceptorGuard](MemoryAnalyzerEngine/InterceptorGuard.h)** / **`TrackingGuard`** are thread-local reentrancy guards so the tracker's own allocations (map nodes, `std::cout`, `addr2line`'s pipe buffer, ...) don't recursively re-trigger tracking.

### Link-time mode (`MemoryAnalyzerCore`)

- **[NewDelete.cpp](MemoryAnalyzerCore/NewDelete.cpp)** overrides every `operator new`/`delete` variant (plain, array, nothrow, aligned), performs the allocation via `PlatformMemory`, captures a stack trace and pushes an event onto the queue.
- **[Runtime](MemoryAnalyzerCore/Runtime.h)** auto-starts everything via a static initializer and registers an `atexit` handler to print the leak report - linking `MemoryAnalyzerCore` into a project is meant to be enough, no code changes required.

### Injection mode (`MemoryAnalyzer`)

Shared by both platforms: **[Session](MemoryAnalyzer/Session.h)** is the injection counterpart of `Runtime` (purely lazy initialization on the first hooked call, declining to track once shutdown has completed), and **[Logging](MemoryAnalyzer/Logging.cpp)** writes the leak report to a file rather than the target's stdout: `$MEMANALYZER_LOG_PATH` if set, otherwise `$TMPDIR`/`$TEMP` (or `/tmp`)`/memoryanalyzer-<pid>.log`. On Linux the path is announced on stderr.

#### Linux

A `MODULE` library (`libMemoryAnalyzer.so`) loaded with `LD_PRELOAD`:

- **[MallocHooks.cpp](MemoryAnalyzer/Linux/MallocHooks.cpp)** overrides `malloc`/`free`/`calloc`/`realloc`; **[NewDeleteHooks.cpp](MemoryAnalyzer/NewDeleteHooks.cpp)** overrides the `operator new`/`delete` set, same as Core.
- **[RealAllocator](MemoryAnalyzer/Linux/RealAllocator.h)** does the real allocation work through `dlsym(RTLD_NEXT, ...)`-resolved functions (calling `malloc` by name would bind back to the hooks), with a small static bootstrap buffer to survive `dlsym` calling `calloc` during resolution.

Usage:

```bash
MEMANALYZER_LOG_PATH=leaks.log LD_PRELOAD=/path/to/libMemoryAnalyzer.so ./your-program
```

#### Windows

`MemoryAnalyzer.dll` plus **`MemoryAnalyzerLauncher.exe`**, which starts the target suspended, injects the DLL with a remote `LoadLibraryW` thread, then resumes it - so hooks are in place before the target's own CRT startup runs:

- **[AllocHooks.cpp](MemoryAnalyzer/Windows/AllocHooks.cpp)** patches the import address table of every loaded module (matching by the real function's address), redirecting the UCRT's `malloc`/`free`/`calloc`/`realloc`/`_aligned_malloc`/`_aligned_free` (and the `_dbg` variants) to tracking hooks. `operator new`/`delete` need no hooks of their own - they bottom out in these calls, so each allocation is seen exactly once. Release (`ucrtbase.dll`) and debug (`ucrtbased.dll`) CRTs are hooked independently. `LoadLibrary*` is hooked too, so modules loaded later are patched, and `ExitProcess` is hooked so the leak report runs while the tracker thread is still alive.
- **[Launcher.cpp](MemoryAnalyzer/Windows/Launcher.cpp)** forwards the target's arguments, stdio handles and exit code.

```bat
set MEMANALYZER_LOG_PATH=leaks.log
MemoryAnalyzerLauncher.exe MemoryAnalyzer.dll your-program.exe [args...]
```

Limits: a target with a statically linked CRT carries its own `malloc` and is invisible to IAT patching; delay-loaded imports and `GetProcAddress`-resolved pointers aren't patched; attaching to an already-running process isn't supported (a late attach would miss earlier allocations).

## What this can and can't catch

This is a deliberately scoped, link-time tool, not a full memory debugger:

- `MemoryAnalyzerCore` only intercepts C++ allocation (`operator new`/`delete` and their variants) - raw `malloc`/`free`/`calloc`/`realloc` and anything allocated by other libraries outside `operator new` is out of reach here, and deliberately so: `operator new`/`delete` are portably replaceable *by the C++ standard itself*, which is exactly why a plain static library can override them uniformly across MSVC/GCC/Clang with no platform-specific code. `malloc` has no equivalent standard replacement mechanism - overriding it always means adopting some form of load-time interposition instead (glibc's undocumented `__libc_malloc` + `dlsym(RTLD_NEXT, ...)` via `LD_PRELOAD` on Linux, IAT patching on Windows, `DYLD_INTERPOSE` via `DYLD_INSERT_LIBRARIES` on macOS). That's a fundamentally different mechanism than "link a static lib and it works," and it's exactly the mechanism the injection-based `MemoryAnalyzer` DLL is for - `malloc` support belongs there, not here. (One thing worth carrying over when that work starts: `operator new` on Linux/libstdc++ typically calls `malloc` internally, so hooking both in the same process would double-count a single `new` as two events unless something suppresses the inner one - the `InterceptorGuard`/`TrackingGuard` reentrancy guards already in Core happen to solve exactly this, being thread-local and scoped to the whole call chain rather than one function, so that piece likely ports over as-is.) Lower-level allocation (`mmap`/`VirtualAlloc`, a custom arena/pool allocator, a `std::pmr` resource backed by its own buffer, an embedded interpreter's own allocator) is out of reach for any generic tool unless it happens to route through `operator new`/`malloc` internally - not something either half of this project can chase in general.
- Very early allocations (from another translation unit's global constructor that happens to run before this library's own static initializer) are still tracked correctly: `NewDelete.cpp`'s `trackAllocation`/`trackDeallocation` self-initialize on first use rather than relying on winning the static-init-order race, which the `#pragma init_seg(lib)` / `__attribute__((init_priority))` hints on `RuntimeInitializer` (see [Runtime.cpp](MemoryAnalyzerCore/Runtime.cpp)) can narrow but never guarantee - confirmed in practice on Darwin, where neither hint applies at all. The corresponding shutdown-side hazard - a *different* translation unit's global deallocating during its own teardown, after this library's own tracking machinery has already been torn down - is handled the same way, by cleanly declining to track rather than touching already-destroyed state (see `Runtime::isShutdownComplete()`).
- A global/static object that's intentionally alive for the whole process lifetime (never explicitly freed, but not actually a bug) will show up in the report as a leak - this tool doesn't yet distinguish "definitely lost" from "still reachable" the way tools like LeakSanitizer do.
- Only single-module coverage: allocations inside a different module (a DLL/shared library that doesn't itself link `MemoryAnalyzerCore`) are invisible to it. That's the gap the injection-based `MemoryAnalyzer` library is meant to close (Linux today; Windows and macOS planned).

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
| `MemoryAnalyzerEngine/` | Static library with the platform-independent tracking engine (queue, tracker thread, stack capture/symbolization, reentrancy guards), shared by both modes. |
| `MemoryAnalyzerCore/` | The link-time mode: `operator new`/`delete` overrides plus auto-start runtime - link it into any target. |
| `MemoryAnalyzer/` | The injection mode: `LD_PRELOAD` library on Linux, launcher + injected DLL on Windows (the target is empty on macOS for now). `MemoryAnalyzer/tests/` holds `LeakyTarget` and the CTest scenarios that run it under the injector and check the log. |
| `TestApp/` | A manual smoke test exercising every allocation form (plain/array/nothrow/aligned, matched frees, deliberate leaks). |
| `tests/` | Automated tests: `Tracker` unit tests, and end-to-end lifetime scenarios (global objects, function-local statics, cross-thread allocation) verified against the real interception pipeline via CTest. |

## Status

- ✅ Core interception, tracking, and symbolized reporting - Windows, Linux, and macOS, tested end-to-end via CI on real hardware for each (MSVC, clang-cl, GCC, Clang, Apple Clang).
- ✅ Automated tests covering allocation forms, object lifetimes, and multi-threaded allocation.
- ✅ Shared engine extracted into `MemoryAnalyzerEngine`; Core is now a thin layer over it.
- ✅ Linux injection library (`LD_PRELOAD`): `malloc`/`free`/`calloc`/`realloc` and `operator new`/`delete`, with end-to-end CTest scenarios (freed/leaked malloc, `new[]`, calloc, realloc, background threads).
- ⏳ Windows injection (DLL injection + IAT patching) - not started.
- ⏳ macOS injection (`DYLD_INSERT_LIBRARIES` + `DYLD_INTERPOSE`) - not started.
