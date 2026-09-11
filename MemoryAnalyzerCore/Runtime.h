#include "EventQueue.h"

class Runtime
{
public:
    static void initialize();
    static void shutdown();

    static bool isInitialized();
    static EventQueue& getEventQueue();

private:
    static bool initialized;
};