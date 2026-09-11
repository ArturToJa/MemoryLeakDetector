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

void Tracker::onDeallocate(void* address, std::size_t size)
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

    if (size != 0 && size != it->second.size)
    {
        std::cout
            << "[SIZE MISMATCH] "
            << address
            << " | freed as "
            << size
            << " bytes but allocated as "
            << it->second.size
            << " bytes\n";
    }

    std::cout
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
    std::cout << "\n========== LEAK REPORT ==========\n";

    auto leaks = getLeaks();

    if (leaks.empty())
    {
        std::cout << "No leaks detected.\n";
        return;
    }

    std::size_t total = 0;

    for (const auto& leak : leaks)
    {
        std::cout
            << "[LEAK] "
            << leak.address
            << " | "
            << leak.size
            << " bytes\n";

        std::cout << "  Stack trace:\n";

        auto symbolizedTrace =
            PlatformStackTrace::symbolize(leak.stackTrace);

        std::cout
            << PlatformStackTrace::format(symbolizedTrace);

        total += leak.size;
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