#pragma once

#include <cstddef>
#include <unordered_map>
#include "Platform/StackTrace/PlatformStackTrace.h"

class Tracker
{
public:
    void onAllocate(
        void* address,
        std::size_t size,
        const PlatformStackTrace::StackTrace& stackTrace);
    void onDeallocate(void* address);

    void reportLeaks() const;

private:
    struct AllocationInfo
    {
        std::size_t size;
        PlatformStackTrace::StackTrace stackTrace;
    };

    std::unordered_map<void*, AllocationInfo> allocations;
};

Tracker& getTracker();

class TrackingGuard
{
public:
    TrackingGuard();
    ~TrackingGuard();

    static bool isTrackingDisabled();

private:
    static thread_local bool disabled;
};