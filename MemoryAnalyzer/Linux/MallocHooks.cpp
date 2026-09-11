// The malloc/free/calloc/realloc overrides that LD_PRELOAD actually
// interposes over libc's. This is the coverage MemoryAnalyzerCore
// structurally cannot reach - it only intercepts operator new/delete (see
// the README's "what this can and can't catch" section for why). Real
// allocation work is always done via RealAllocator (dlsym(RTLD_NEXT, ...)
// -resolved), never by calling malloc()/free() by name here, since under
// LD_PRELOAD's global symbol interposition those bare names would bind
// back to these very functions.

#include "RealAllocator.h"
#include "../Session.h"
#include "../Events.h"
#include "InterceptorGuard.h"
#include "Tracker.h"

#include <cstddef>

namespace
{
    void trackAllocation(void* ptr, std::size_t size)
    {
        if (!ptr || RealAllocator::isInternalBootstrapPointer(ptr))
            return;

        if (InterceptorGuard::isDisabled() || TrackingGuard::isTrackingDisabled())
            return;

        if (!Session::ensureInitialized())
            return;

        InterceptorGuard guard;

        AllocationEvent event{
            EventType::Allocate,
            ptr,
            size,
            PlatformStackTrace::capture(1)
        };

        Session::getEventQueue().push(event);
    }

    void trackDeallocation(void* ptr)
    {
        if (!ptr || RealAllocator::isInternalBootstrapPointer(ptr))
            return;

        if (InterceptorGuard::isDisabled() || TrackingGuard::isTrackingDisabled())
            return;

        if (!Session::ensureInitialized())
            return;

        InterceptorGuard guard;

        AllocationEvent event{
            EventType::Deallocate,
            ptr,
            0,
            {}
        };

        Session::getEventQueue().push(event);
    }
}

extern "C" void* malloc(std::size_t size)
{
    void* ptr = RealAllocator::malloc(size);
    trackAllocation(ptr, size);
    return ptr;
}

extern "C" void free(void* ptr)
{
    trackDeallocation(ptr);
    RealAllocator::free(ptr);
}

extern "C" void* calloc(std::size_t count, std::size_t size)
{
    void* ptr = RealAllocator::calloc(count, size);
    trackAllocation(ptr, count * size);
    return ptr;
}

extern "C" void* realloc(void* ptr, std::size_t size)
{
    void* oldPtr = ptr;
    void* newPtr = RealAllocator::realloc(ptr, size);

    if (oldPtr && newPtr && newPtr != oldPtr)
        trackDeallocation(oldPtr);

    trackAllocation(newPtr, size);

    return newPtr;
}
