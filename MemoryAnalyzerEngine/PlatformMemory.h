#pragma once

#include <cstddef>

namespace PlatformMemory
{
    void* allocate(std::size_t size);
    void deallocate(void* ptr);

    void* allocateAligned(
        std::size_t size,
        std::size_t alignment);

    void deallocateAligned(void* ptr);
}