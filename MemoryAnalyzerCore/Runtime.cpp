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
    struct RuntimeInitializer
    {
        RuntimeInitializer()
        {
            Runtime::initialize();
        }
    };

    RuntimeInitializer runtimeInitializer;
}