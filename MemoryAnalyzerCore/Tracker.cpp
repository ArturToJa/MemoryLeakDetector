#include "pch.h"
#include "Tracker.h"

#include <iostream>

thread_local bool TrackingGuard::disabled = false;

TrackingGuard::TrackingGuard()
{
    disabled = true;
}

TrackingGuard::~TrackingGuard()
{
    disabled = false;
}

bool TrackingGuard::isTrackingDisabled()
{
    return disabled;
}

void Tracker::onAllocate(
    void* address,
    std::size_t size,
    const PlatformStackTrace::StackTrace& stackTrace)
{
    if (TrackingGuard::isTrackingDisabled())
        return;

    TrackingGuard guard;

    allocations[address] = {
        size,
        stackTrace
    };

    std::cout
        << "[ALLOC] "
        << address
        << " | "
        << size
        << " bytes\n";
}

void Tracker::onDeallocate(void* address)
{
    if (TrackingGuard::isTrackingDisabled())
        return;

    TrackingGuard guard;

    auto it = allocations.find(address);

    if (it == allocations.end())
    {
        std::cout
            << "[FREE?] "
            << address
            << " | unknown allocation\n";

        return;
    }

    std::cout
        << "[FREE] "
        << address
        << " | "
        << it->second.size
        << " bytes\n";

    allocations.erase(it);
}

void Tracker::reportLeaks() const
{
    std::cout << "\n========== LEAK REPORT ==========\n";

    if (allocations.empty())
    {
        std::cout << "No leaks detected.\n";
        return;
    }

    std::size_t total = 0;

    for (const auto& [address, info] : allocations)
    {
        std::cout
            << "[LEAK] "
            << address
            << " | "
            << info.size
            << " bytes\n";

        std::cout << "  Stack trace:\n";

        auto symbolizedTrace =
            PlatformStackTrace::symbolize(info.stackTrace);

        std::cout
            << PlatformStackTrace::format(symbolizedTrace);

        total += info.size;
    }

    std::cout
        << "Total leaked: "
        << total
        << " bytes\n";
}

Tracker& getTracker()
{
    static Tracker tracker;
    return tracker;
}