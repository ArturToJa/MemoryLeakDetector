#include "Tracker.h"

#include <cstdio>

namespace
{
    int failures = 0;

    void expect(bool condition, const char* message)
    {
        if (!condition)
        {
            std::fprintf(stderr, "FAILED: %s\n", message);
            ++failures;
        }
    }
}

int main()
{
    // A freed allocation should not show up as a leak; an unfreed one should.
    {
        Tracker tracker;

        int a = 0;
        int b = 0;

        tracker.onAllocate(&a, 4, {});
        tracker.onAllocate(&b, 8, {});
        tracker.onDeallocate(&a, 4);

        auto leaks = tracker.getLeaks();

        expect(leaks.size() == 1,
            "exactly one leak remains after freeing one of two allocations");
        expect(!leaks.empty() && leaks[0].address == &b,
            "the remaining leak is the one that was never freed");
        expect(!leaks.empty() && leaks[0].size == 8,
            "the remaining leak reports its original size");
    }

    // Freeing an address that was never tracked must not crash or affect state.
    {
        Tracker tracker;

        int a = 0;
        void* untracked = reinterpret_cast<void*>(0x1234);

        tracker.onAllocate(&a, 4, {});
        tracker.onDeallocate(untracked, 4);

        auto leaks = tracker.getLeaks();

        expect(leaks.size() == 1,
            "freeing an unknown address does not remove a real allocation");
    }

    // A sized-delete whose size disagrees with the tracked allocation size
    // is a mismatch worth logging, but the allocation is still considered
    // freed rather than left dangling as a false-positive leak.
    {
        Tracker tracker;

        int a = 0;

        tracker.onAllocate(&a, 4, {});
        tracker.onDeallocate(&a, 999);

        auto leaks = tracker.getLeaks();

        expect(leaks.empty(),
            "a size-mismatched free still removes the allocation");
    }

    // Nothing allocated -> nothing leaked.
    {
        Tracker tracker;

        expect(tracker.getLeaks().empty(),
            "a tracker with no allocations reports no leaks");
    }

    if (failures == 0)
    {
        std::printf("All Tracker tests passed.\n");
        return 0;
    }

    std::fprintf(stderr, "%d Tracker test(s) failed.\n", failures);
    return 1;
}
