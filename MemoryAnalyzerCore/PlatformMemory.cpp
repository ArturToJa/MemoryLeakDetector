#include "pch.h"
#include "PlatformMemory.h"

#include <cstdlib>

#ifdef _WIN32
#include <malloc.h>
#endif

namespace PlatformMemory
{
    void* allocate(std::size_t size)
    {
        return std::malloc(size);
    }

    void deallocate(void* ptr)
    {
        std::free(ptr);
    }

#ifdef _WIN32

    void* allocateAligned(
        std::size_t size,
        std::size_t alignment)
    {
        return _aligned_malloc(size, alignment);
    }

    void deallocateAligned(void* ptr)
    {
        _aligned_free(ptr);
    }

#else

    void* allocateAligned(
        std::size_t size,
        std::size_t alignment)
    {
        // posix_memalign requires alignment to be a power of two and at
        // least sizeof(void*); std::align_val_t from operator new is
        // always a valid alignment already, but never smaller than that.
        if (alignment < sizeof(void*))
            alignment = sizeof(void*);

        void* ptr = nullptr;

        if (posix_memalign(&ptr, alignment, size) != 0)
            return nullptr;

        return ptr;
    }

    void deallocateAligned(void* ptr)
    {
        std::free(ptr);
    }

#endif
}