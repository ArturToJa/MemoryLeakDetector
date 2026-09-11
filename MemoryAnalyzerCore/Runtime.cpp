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

namespace
{
    // Starts the tracker at static-init time. Because C++ only guarantees
    // initialization order within a single translation unit, any allocation
    // made by a *different* TU's global/static constructor that happens to
    // run before this one (link order is otherwise unspecified) will not be
    // tracked - trackAllocation/trackDeallocation in NewDelete.cpp silently
    // no-op until Runtime::isInitialized() is true. In practice this means
    // very early, static-init-time allocations are a known blind spot.
    struct RuntimeInitializer
    {
        RuntimeInitializer()
        {
            Runtime::initialize();
        }
    };

    RuntimeInitializer runtimeInitializer;
}