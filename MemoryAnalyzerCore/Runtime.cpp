#include "pch.h"
#include "Runtime.h"
#include "EventQueue.h"
#include "Tracker.h"
#include "TrackerThread.h"
#include "Platform/StackTrace/SymbolResolver.h"

#include <cstdlib>
#include <iostream>

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
}

bool Runtime::initialized = false;

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
    if (initialized)
        return;

    getTracker();
    getEventQueueInternal();

    SymbolResolver::instance().initialize();

    getTrackerThread().start();

    std::atexit(&Runtime::shutdown);

    initialized = true;
}

void Runtime::shutdown()
{
    if (!initialized)
        return;

    initialized = false;

    getTrackerThread().stop();
    getTracker().reportLeaks();

    SymbolResolver::instance().shutdown();
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
    // initialization order within a single translation unit, any allocation
    // made by a *different* TU's global/static constructor that happens to
    // run before this one will not be tracked - trackAllocation/
    // trackDeallocation in NewDelete.cpp silently no-op until
    // Runtime::isInitialized() is true. The init_seg/init_priority hints
    // above narrow that race by making this run as early as the toolchain
    // permits, but they cannot guarantee it wins against another library
    // doing the same trick, nor against CRT/loader-level allocation that
    // happens before any C++ global constructor runs at all, nor against
    // allocations made inside other modules (DLLs/shared libraries) that
    // don't link this static library - those are out of reach here by
    // design (see the injection-based MemoryAnalyzer DLL for that case).
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