#pragma once

#include <cstddef>
#include <iostream>
#include <unordered_map>
#include <vector>
#include "Platform/StackTrace/PlatformStackTrace.h"

struct LeakInfo
{
    void* address;
    std::size_t size;
    PlatformStackTrace::StackTrace stackTrace;
};

class Tracker
{
public:
    // Defaults to std::cout so existing callers (Core's getTracker()
    // singleton) are unaffected; an injected library can instead bind its
    // own Tracker to a log file, since it has no console it can rely on.
    explicit Tracker(std::ostream& output = std::cout)
        : output(output)
    {
    }

    void onAllocate(
        void* address,
        std::size_t size,
        const PlatformStackTrace::StackTrace& stackTrace);

    // `size` is the size passed to a sized-delete overload, or 0 when the
    // caller only knows an unsized delete was used (no mismatch check is
    // possible in that case).
    void onDeallocate(void* address, std::size_t size = 0);

    // Snapshot of everything still allocated. Not thread-safe against
    // concurrent onAllocate/onDeallocate calls - only call this once the
    // tracker thread has been stopped (see Runtime::shutdown).
    std::vector<LeakInfo> getLeaks() const;

    void reportLeaks() const;

private:
    struct AllocationInfo
    {
        std::size_t size;
        PlatformStackTrace::StackTrace stackTrace;
    };

    std::ostream& output;
    std::unordered_map<void*, AllocationInfo> allocations;
};

Tracker& getTracker();

// Reentrancy guard for the *consumer* side: Tracker::onAllocate/onDeallocate
// touch the allocations map and std::cout, both of which can allocate. This
// guard stops that from recursing back into the tracker on the tracker
// thread. See InterceptorGuard in InterceptorGuard.h for the analogous guard
// on the producer side (capturing/queuing events on the allocating thread).
class TrackingGuard
{
public:
    TrackingGuard();
    ~TrackingGuard();

    static bool isTrackingDisabled();

private:
    static thread_local bool disabled;
};