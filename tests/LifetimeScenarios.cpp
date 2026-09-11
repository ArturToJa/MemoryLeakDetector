// This executable exercises MemoryAnalyzerCore's *real* global operator
// new/delete interception end-to-end (unlike TrackerTests.cpp, which drives
// Tracker directly). Each scenario allocates a distinct, unusual byte count
// so the printed "[LEAK] ... | N bytes" report can be checked by exact size
// via CTest's PASS_REGULAR_EXPRESSION / FAIL_REGULAR_EXPRESSION properties
// (see tests/CMakeLists.txt) - no custom output parsing needed.
//
// Why the global/static-duration objects below are expected to NOT show up
// as leaks: their destructors run automatically at program termination,
// and Runtime::shutdown's atexit registration - made as early as possible
// during static init (see Runtime.cpp) - is therefore guaranteed to run
// *after* every ordinary static object's destructor, per the standard's
// reverse-of-registration-order termination rule. By the time the leak
// report is generated, those objects are already gone. A raw pointer that
// is intentionally never freed has no destructor to do that, so it
// correctly (if perhaps surprisingly, to a first-time reader of a leak
// report) shows up as a leak - this is the same "still reachable, not
// actually a bug" category that real tools like LeakSanitizer separate out
// and this tool currently does not.

#include <cstddef>
#include <iostream>
#include <thread>

namespace
{
    struct HeapOwner
    {
        explicit HeapOwner(std::size_t size)
            : data(new char[size])
        {
        }

        ~HeapOwner()
        {
            delete[] data;
        }

        char* data;
    };

    // Global, non-pointer, static storage duration: destructed automatically
    // at program exit, before Runtime::shutdown ever runs. Should NOT leak.
    HeapOwner globalOwnerNotLeaked(149);

    // Global raw pointer: nothing ever runs a destructor for this, so its
    // allocation is never freed. Intentionally leaked to demonstrate the
    // "still reachable at exit" case described above.
    char* globalNeverFreed = new char[151];

    // Meyer's singleton: lazily constructed on first call, destructed
    // automatically at program exit with the same guarantee as
    // globalOwnerNotLeaked above.
    HeapOwner& functionLocalStaticCache()
    {
        static HeapOwner instance(157);
        return instance;
    }
}

int main()
{
    std::cout << "========== LIFETIME SCENARIOS START ==========\n";

    // Local scope: allocated and freed before the scope ends. Should NOT leak.
    {
        char* p = new char[101];
        delete[] p;
    }

    // Local scope: allocated, never freed. Should leak.
    {
        char* p = new char[103];
        (void)p;
    }

    // Force the function-local static singleton to construct.
    (void)functionLocalStaticCache();

    // Allocation made and freed entirely on a background thread, joined
    // before continuing. Should NOT leak - the event still flows through
    // the same shared queue regardless of which thread produced it.
    {
        std::thread worker([]
            {
                char* p = new char[163];
                delete[] p;
            });

        worker.join();
    }

    // Allocation made on a background thread and intentionally never freed,
    // even though the thread itself has since finished and been joined.
    // Should leak - the allocating thread's lifetime is unrelated to the
    // allocation's.
    {
        std::thread worker([]
            {
                char* p = new char[167];
                (void)p;
            });

        worker.join();
    }

    std::cout << "=========== LIFETIME SCENARIOS END ===========\n";

    return 0;
}
