// A deliberately plain, ordinary executable - it must NOT link
// MemoryAnalyzerCore/Engine/MemoryAnalyzer, to genuinely stand in for
// "someone else's already-built binary" that the injection library has
// no special relationship with. Distinctive, unusual byte counts (in the
// same spirit as tests/LifetimeScenarios.cpp) let the resulting log be
// checked per scenario without relying on addresses.

#include <cstdio>
#include <cstdlib>
#include <thread>

int main()
{
    std::printf("LEAKY TARGET START\n");

    // malloc, freed - should NOT leak.
    {
        void* p = std::malloc(211);
        std::free(p);
    }

    // malloc, never freed - should leak.
    {
        void* p = std::malloc(223);
        (void)p;
    }

    // new[], freed - should NOT leak.
    {
        char* p = new char[227];
        delete[] p;
    }

    // new[], never freed - should leak.
    {
        char* p = new char[229];
        (void)p;
    }

    // calloc, never freed - should leak.
    {
        void* p = std::calloc(1, 233);
        (void)p;
    }

    // realloc growing an allocation; the final size is never freed -
    // should leak, reported at the grown size.
    {
        void* p = std::malloc(16);
        p = std::realloc(p, 239);
        (void)p;
    }

    // Allocation made and freed entirely on a background thread, joined
    // before continuing - should NOT leak.
    {
        std::thread worker([]
            {
                void* p = std::malloc(241);
                std::free(p);
            });

        worker.join();
    }

    // Allocation made on a background thread and never freed, even though
    // the thread itself has since finished and been joined - should leak
    // (the allocating thread's lifetime is unrelated to the allocation's).
    {
        std::thread worker([]
            {
                void* p = std::malloc(251);
                (void)p;
            });

        worker.join();
    }

    std::printf("LEAKY TARGET END\n");

    return 0;
}
