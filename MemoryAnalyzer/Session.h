#pragma once

#include "EventQueue.h"

// The injection library's counterpart to MemoryAnalyzerCore's Runtime -
// deliberately simpler, since there's no equivalent of Runtime's
// static-init-order race to fight here: LD_PRELOAD's/DYLD_INSERT_LIBRARIES'
// constructors run before the target's own main() either way, but there's
// still no guaranteed ordering against *other* preloaded/interposed
// libraries, so initialization is purely lazy (first hooked call wins)
// rather than trying to win an eager static-init race the way Runtime does.
namespace Session
{
    // Returns true if tracking is available (already initialized, or just
    // successfully initialized now); false once shutdown has completed, in
    // which case the caller must drop the event rather than touch
    // torn-down state - see MemoryAnalyzerCore's Runtime::isShutdownComplete
    // and the real crash its absence caused (use-after-destroy on
    // EventQueue's mutex, hit during the macOS port) for exactly why this
    // matters.
    bool ensureInitialized();

    EventQueue& getEventQueue();

    // Stops the tracker thread and prints the leak report. Safe to call
    // more than once (and before initialization); only the first call after
    // a successful initialization does anything. Registered via atexit on
    // POSIX; on Windows the injected DLL calls it from its ExitProcess hook.
    void shutdown();
}
