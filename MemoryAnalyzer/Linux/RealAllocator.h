#pragma once

#include <cstddef>

// Resolves and forwards to the *real* (non-intercepted) allocator
// functions via dlsym(RTLD_NEXT, ...). Both NewDeleteHooks.cpp (for
// operator new/delete) and MallocHooks.cpp (for the exported malloc/free/
// calloc/realloc symbols LD_PRELOAD interposes) route their actual
// allocation work through here rather than calling std::malloc/free or
// the bare malloc()/free() names directly - under LD_PRELOAD, a bare call
// to malloc() from anywhere in this shared object, including our own
// operator new override, binds to *our own* exported malloc symbol, not
// libc's, so calling it directly would recurse into our own hook.
namespace RealAllocator
{
    void* malloc(std::size_t size);
    void free(void* ptr);
    void* calloc(std::size_t count, std::size_t size);
    void* realloc(void* ptr, std::size_t size);

    // posix_memalign-backed; the returned pointer is freed with a plain
    // free(), same as any other allocation - POSIX guarantees that.
    void* alignedAlloc(std::size_t size, std::size_t alignment);

    // True for the handful of tiny allocations served from RealAllocator's
    // own internal bootstrap buffer (see RealAllocator.cpp) rather than
    // the real allocator - callers should not report these as user
    // allocations.
    bool isInternalBootstrapPointer(void* ptr);
}
