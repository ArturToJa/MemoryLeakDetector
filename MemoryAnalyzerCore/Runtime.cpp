#include "pch.h"
#include "Runtime.h"
#include "EventQueue.h"
#include "Tracker.h"
#include "TrackerThread.h"
#include "InterceptorGuard.h"
#include "Platform/StackTrace/SymbolResolver.h"

#include <cstdlib>
#include <mutex>

namespace
{
    EventQueue& getEventQueueInternal()
    {
        static EventQueue queue;
        return queue;
    }

    TrackerThread& getTrackerThread()
    {
        static TrackerThread thread(
            Runtime::getEventQueue(),
            getTracker()
        );

        return thread;
    }

    std::once_flag initOnce;
}

std::atomic<bool> Runtime::initialized = false;
std::atomic<bool> Runtime::shutdownComplete = false;

bool Runtime::isInitialized()
{
    return initialized;
}

EventQueue& Runtime::getEventQueue()
{
    return getEventQueueInternal();
}

void Runtime::initialize()
{
    // Once shutdown() has run, never re-initialize - see the comment on
    // shutdownComplete's use in shutdown() below for why this matters.
    if (shutdownComplete)
        return;

    // std::call_once makes this safe against genuinely concurrent calls from
    // different threads (one runs the body, the rest block until it's done).
    // The InterceptorGuard inside the body is a separate concern: it stops
    // this function's own internal allocations (Tracker's map, the event
    // queue, the tracker thread) from being mistaken for user allocations
    // and re-entering this same call on the SAME thread via
    // NewDelete.cpp's self-initializing fallback - without it, the very
    // first allocation made by e.g. Tracker's constructor would trigger
    // that fallback, which would call back into this still-in-progress,
    // not-yet-initialized function and deadlock on getTracker()'s
    // function-local-static guard (observed in practice, not hypothetical).
    std::call_once(initOnce, []
    {
        InterceptorGuard guard;

        getTracker();
        getEventQueueInternal();

        SymbolResolver::instance().initialize();

        getTrackerThread().start();

        std::atexit(&Runtime::shutdown);

        initialized = true;
    });
}

void Runtime::shutdown()
{
    if (!initialized)
        return;

    initialized = false;

    getTrackerThread().stop();
    getTracker().reportLeaks();

    SymbolResolver::instance().shutdown();

    // NewDelete.cpp's self-initializing fallback means *any* allocation or
    // deallocation, anywhere, can call Runtime::initialize() again - and
    // other globals' destructors (in other TUs, whose relative order versus
    // this one is unspecified) can still run after this point and do
    // exactly that. Without this latch, such a call would try to reuse
    // initOnce/the tracker thread/etc. mid-teardown, which is exactly the
    // "mutex lock failed: Invalid argument" libc++abi crash observed in
    // practice on macOS during static destruction. Past this point,
    // tracking is simply over - there's nothing left to report to.
    shutdownComplete = true;
}

// Push this translation unit's dynamic initializers as early as the
// platform allows, to narrow (not eliminate) the static-init-order race
// described below. On MSVC, "lib" is the segment Microsoft documents as
// intended for third-party library initialization - it runs before ordinary
// (default "user" segment) global constructors elsewhere in the same
// module, but after the CRT's own reserved "compiler" segment. There is no
// portable equivalent of this pragma outside MSVC, so non-MSVC compilers
// use __attribute__((init_priority(...))) on the object itself instead (see
// below); that attribute is unsupported on Darwin/Mach-O, so macOS gets no
// extra help here beyond ordinary link order.
#if defined(_MSC_VER)
#pragma init_seg(lib)
#endif

namespace
{
    // Starts the tracker at static-init time. Because C++ only guarantees
    // initialization order within a single translation unit, a different
    // TU's global/static constructor can still run - and allocate - before
    // this one, and the init_seg/init_priority hints above can't guarantee
    // otherwise (observed in practice on Darwin, where neither applies at
    // all). That used to mean the allocation was silently untracked;
    // trackAllocation/trackDeallocation in NewDelete.cpp now self-initialize
    // on first use instead, so this constructor winning the race is purely
    // an optimization (the tracker thread gets started slightly sooner)
    // rather than a correctness requirement. What's still genuinely out of
    // reach: CRT/loader-level allocation that happens before any C++ global
    // constructor runs at all, and allocations made inside other modules
    // (DLLs/shared libraries) that don't link this static library - see the
    // injection-based MemoryAnalyzer DLL for that case.
    struct RuntimeInitializer
    {
        RuntimeInitializer()
        {
            Runtime::initialize();
        }
    };

#if defined(__GNUC__) && !defined(__APPLE__)
    RuntimeInitializer runtimeInitializer __attribute__((init_priority(101)));
#else
    RuntimeInitializer runtimeInitializer;
#endif
}