#include "Tracker.h"

#include <cstdio>
#include <sstream>
#include <string>

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

    // A Tracker bound to a custom sink writes there, not to std::cout -
    // this is what lets an injected library redirect reporting to a log
    // file instead of a console it may not have.
    {
        std::ostringstream sink;
        Tracker tracker(sink);

        int a = 0;

        tracker.onAllocate(&a, 4, {});
        tracker.reportLeaks();

        expect(sink.str().find("[ALLOC]") != std::string::npos,
            "a Tracker bound to a custom sink writes onAllocate output there");
        expect(sink.str().find("[LEAK]") != std::string::npos,
            "a Tracker bound to a custom sink writes reportLeaks output there");
    }

    // Allocations of the same size from the same call stack collapse into one
    // report entry with a count; a different size or a different stack stays
    // separate; the grand total still counts every individual allocation;
    // and the biggest offender (by total bytes) is listed first.
    {
        std::ostringstream sink;
        Tracker tracker(sink);

        int repeated[3] = {};
        int otherSize = 0;
        int otherStack = 0;

        tracker.onAllocate(&repeated[0], 16, {1, 2});
        tracker.onAllocate(&repeated[1], 16, {1, 2});
        tracker.onAllocate(&repeated[2], 16, {1, 2});
        tracker.onAllocate(&otherSize, 32, {1, 2});
        tracker.onAllocate(&otherStack, 16, {3, 4});
        tracker.reportLeaks();

        // The sink also holds the [ALLOC] lines written along the way, so
        // look only at the report itself.
        const std::string full = sink.str();
        const std::size_t reportStart = full.find("LEAK REPORT");

        expect(reportStart != std::string::npos, "the report has its header");

        const std::string report = full.substr(reportStart == std::string::npos ? 0 : reportStart);

        std::size_t leakLines = 0;

        for (std::size_t at = report.find("[LEAK]"); at != std::string::npos;
             at = report.find("[LEAK]", at + 1))
        {
            ++leakLines;
        }

        expect(leakLines == 3,
            "five allocations from three distinct (size, stack) groups produce three [LEAK] entries");
        expect(report.find("| 16 bytes (x3, 48 bytes total)") != std::string::npos,
            "a repeated leak is reported once with its count and total");
        expect(report.find("| 32 bytes\n") != std::string::npos,
            "a leak of a different size is reported on its own, without a count");
        expect(report.find("Total leaked: 96 bytes in 5 allocations") != std::string::npos,
            "the total still covers every individual allocation");
        expect(report.find("(x3") < report.find("| 32 bytes"),
            "the group with the most leaked bytes is listed first");
    }

    if (failures == 0)
    {
        std::printf("All Tracker tests passed.\n");
        return 0;
    }

    std::fprintf(stderr, "%d Tracker test(s) failed.\n", failures);
    return 1;
}
