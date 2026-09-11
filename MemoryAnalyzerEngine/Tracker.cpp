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

    output
        << "[ALLOC] "
        << address
        << " | "
        << size
        << " bytes\n";
}

void Tracker::onDeallocate(void* address, std::size_t size)
{
    if (TrackingGuard::isTrackingDisabled())
        return;

    TrackingGuard guard;

    auto it = allocations.find(address);

    if (it == allocations.end())
    {
        output
            << "[FREE?] "
            << address
            << " | unknown allocation\n";

        return;
    }

    if (size != 0 && size != it->second.size)
    {
        output
            << "[SIZE MISMATCH] "
            << address
            << " | freed as "
            << size
            << " bytes but allocated as "
            << it->second.size
            << " bytes\n";
    }

    output
        << "[FREE] "
        << address
        << " | "
        << it->second.size
        << " bytes\n";

    allocations.erase(it);
}

std::vector<LeakInfo> Tracker::getLeaks() const
{
    std::vector<LeakInfo> leaks;
    leaks.reserve(allocations.size());

    for (const auto& [address, info] : allocations)
    {
        leaks.push_back({
            address,
            info.size,
            info.stackTrace
            });
    }

    return leaks;
}

void Tracker::reportLeaks() const
{
    output << "\n========== LEAK REPORT ==========\n";

    auto leaks = getLeaks();

    if (leaks.empty())
    {
        output << "No leaks detected.\n";
        return;
    }

    std::size_t total = 0;

    for (const auto& leak : leaks)
    {
        output
            << "[LEAK] "
            << leak.address
            << " | "
            << leak.size
            << " bytes\n";

        output << "  Stack trace:\n";

        auto symbolizedTrace =
            PlatformStackTrace::symbolize(leak.stackTrace);

        output
            << PlatformStackTrace::format(symbolizedTrace);

        total += leak.size;
    }

    output
        << "Total leaked: "
        << total
        << " bytes\n";
}

Tracker& getTracker()
{
    static Tracker tracker;
    return tracker;
}