#include "pch.h"
#include "Tracker.h"

#include <algorithm>
#include <functional>
#include <iostream>
#include <map>
#include <utility>

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

    // Allocations of the same size made from the same call stack are the
    // same leak repeated (typically a loop), so report them once with a
    // count rather than once each. Symbolizing is the expensive part of a
    // report (one process spawn per frame on Linux/macOS), so this also
    // avoids doing it again for every repeat.
    struct LeakGroup
    {
        const LeakInfo* representative; // lowest address in the group
        std::size_t size;
        std::size_t count;
    };

    using GroupKey = std::pair<std::size_t, PlatformStackTrace::StackTrace>;

    std::map<GroupKey, LeakGroup> grouped;

    std::size_t total = 0;

    for (const auto& leak : leaks)
    {
        auto [it, inserted] = grouped.try_emplace(
            GroupKey{leak.size, leak.stackTrace},
            LeakGroup{&leak, leak.size, 0});

        LeakGroup& group = it->second;

        if (leak.address < group.representative->address)
            group.representative = &leak;

        ++group.count;
        total += leak.size;
    }

    std::vector<LeakGroup> groups;
    groups.reserve(grouped.size());

    for (const auto& entry : grouped)
        groups.push_back(entry.second);

    // Biggest offenders first; the address tiebreak makes the order (and so
    // the report) deterministic, which the allocation map's hash order is not.
    std::sort(groups.begin(), groups.end(),
        [](const LeakGroup& a, const LeakGroup& b)
        {
            const std::size_t aTotal = a.size * a.count;
            const std::size_t bTotal = b.size * b.count;

            if (aTotal != bTotal)
                return aTotal > bTotal;

            return std::less<void*>{}(a.representative->address, b.representative->address);
        });

    for (const auto& group : groups)
    {
        output
            << "[LEAK] "
            << group.representative->address
            << " | "
            << group.size
            << " bytes";

        if (group.count > 1)
        {
            output
                << " (x" << group.count
                << ", " << group.size * group.count
                << " bytes total)";
        }

        output << "\n";

        output << "  Stack trace:\n";

        auto symbolizedTrace =
            PlatformStackTrace::symbolize(group.representative->stackTrace);

        output
            << PlatformStackTrace::format(symbolizedTrace);
    }

    output
        << "Total leaked: "
        << total
        << " bytes in "
        << leaks.size()
        << (leaks.size() == 1 ? " allocation" : " allocations")
        << "\n";
}

Tracker& getTracker()
{
    static Tracker tracker;
    return tracker;
}