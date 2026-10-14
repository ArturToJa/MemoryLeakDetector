// A tiny DLL the LoadLibrary scenario loads *after* injection. It exists so
// that its own import table - which the injector can only have patched if
// its LoadLibrary hook fired - is the sole path its allocations take.

#include <windows.h>

#include <cstdlib>

extern "C" __declspec(dllexport) void leakInPlugin()
{
    // Freed - must NOT be reported.
    void* freed = std::malloc(3003);
    std::free(freed);

    // Never freed - must be reported (malloc, then new[]).
    void* leakedMalloc = std::malloc(3001);
    (void)leakedMalloc;

    char* leakedArray = new char[3007];
    (void)leakedArray;
}
