#include "Session.h"
#include "Logging.h"
#include "Tracker.h"
#include "TrackerThread.h"
#include "InterceptorGuard.h"
#include "Platform/StackTrace/SymbolResolver.h"

#include <atomic>
#include <cstdlib>
#include <mutex>

namespace
{
    EventQueue& getEventQueueInternal()
    {
        static EventQueue queue;
        return queue;
    }

    // Bound to the log file (not std::cout) - see Tracker.h/Logging.h for
    // why: an injected library is inside a target process that may have no
    // usable console at all.
    Tracker& getSessionTracker()
    {
        static Tracker tracker(Logging::stream());
        return tracker;
    }

    TrackerThread& getTrackerThread()
    {
        static TrackerThread thread(
            getEventQueueInternal(),
            getSessionTracker()
        );

        return thread;
    }

    std::once_flag initOnce;
    std::atomic<bool> initialized{false};
    std::atomic<bool> shutdownComplete{false};

    void shutdown()
    {
        if (!initialized)
            return;

        initialized = false;

        getTrackerThread().stop();
        getSessionTracker().reportLeaks();

        SymbolResolver::instance().shutdown();

        // See Session.h - once this is set, ensureInitialized() refuses to
        // touch any of the above again, regardless of which allocation or
        // deallocation, from any thread or any other library torn down
        // later, tries to trigger it.
        shutdownComplete = true;
    }

    void doInitialize()
    {
        InterceptorGuard guard;

        getSessionTracker();
        getEventQueueInternal();

        SymbolResolver::instance().initialize();

        getTrackerThread().start();

        std::atexit(&shutdown);

        initialized = true;
    }
}

namespace Session
{
    bool ensureInitialized()
    {
        if (initialized)
            return true;

        if (shutdownComplete)
            return false;

        std::call_once(initOnce, doInitialize);

        return initialized;
    }

    EventQueue& getEventQueue()
    {
        return getEventQueueInternal();
    }
}
