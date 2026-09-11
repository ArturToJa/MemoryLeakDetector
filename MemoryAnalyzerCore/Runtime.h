#include "EventQueue.h"

#include <atomic>

class Runtime
{
public:
    static void initialize();
    static void shutdown();

    static bool isInitialized();
    static EventQueue& getEventQueue();

private:
    static std::atomic<bool> initialized;
    static std::atomic<bool> shutdownComplete;
};