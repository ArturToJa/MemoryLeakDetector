#include "pch.h"
#include "PlatformMemory.h"

#include <cstdlib>
#include <malloc.h>

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
}