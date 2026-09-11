// MemoryAnalyzerCore only intercepts C++ allocation (operator new/delete and
// their sized/aligned/nothrow variants). Raw malloc/free, C libraries, and
// third-party code that allocates outside of operator new are out of scope
// here by design - link-time interception can only override symbols the
// linker actually resolves through this library. Broader, allocator-agnostic
// interception (e.g. hooking malloc/free at the process level) belongs to
// the injection-based MemoryAnalyzer DLL instead.

#include "pch.h"
#include "Runtime.h"
#include "Tracker.h"
#include "Events.h"
#include "InterceptorGuard.h"
#include "PlatformMemory.h"

#include <cstddef>
#include <cstdlib>
#include <new>

namespace
{
    void trackAllocation(void* ptr, std::size_t size)
    {
        if (!ptr)
            return;

        if (!Runtime::isInitialized() ||
            InterceptorGuard::isDisabled() ||
            TrackingGuard::isTrackingDisabled())
        {
            return;
        }

        InterceptorGuard guard;

        AllocationEvent event{
            EventType::Allocate,
            ptr,
            size,
            PlatformStackTrace::capture(1)
        };

        Runtime::getEventQueue().push(event);
    }

    void trackDeallocation(void* ptr, std::size_t size)
    {
        if (!ptr)
            return;

        if (!Runtime::isInitialized() ||
            InterceptorGuard::isDisabled() ||
            TrackingGuard::isTrackingDisabled())
        {
            return;
        }

        InterceptorGuard guard;

        AllocationEvent event{
            EventType::Deallocate,
            ptr,
            size,
            {}
        };

        Runtime::getEventQueue().push(event);
    }
}

void* operator new(std::size_t size)
{
    void* ptr = PlatformMemory::allocate(size);

    if (!ptr)
        throw std::bad_alloc();

    trackAllocation(ptr, size);

    return ptr;
}

void* operator new[](std::size_t size)
{
    void* ptr = PlatformMemory::allocate(size);

    if (!ptr)
        throw std::bad_alloc();

    trackAllocation(ptr, size);

    return ptr;
}

void operator delete(void* ptr) noexcept
{
    trackDeallocation(ptr, 0);
    PlatformMemory::deallocate(ptr);
}

void operator delete[](void* ptr) noexcept
{
    trackDeallocation(ptr, 0);
    PlatformMemory::deallocate(ptr);
}

void operator delete(void* ptr, std::size_t size) noexcept
{
    trackDeallocation(ptr, size);
    PlatformMemory::deallocate(ptr);
}

void operator delete[](void* ptr, std::size_t size) noexcept
{
    trackDeallocation(ptr, size);
    PlatformMemory::deallocate(ptr);
}

void* operator new(
    std::size_t size,
    const std::nothrow_t&) noexcept
{
    try
    {
        return ::operator new(size);
    }
    catch (...)
    {
        return nullptr;
    }
}

void* operator new[](
    std::size_t size,
    const std::nothrow_t&) noexcept
{
    try
    {
        return ::operator new[](size);
    }
    catch (...)
    {
        return nullptr;
    }
}

void operator delete(
    void* ptr,
    const std::nothrow_t&) noexcept
{
    ::operator delete(ptr);
}

void operator delete[](
    void* ptr,
    const std::nothrow_t&) noexcept
{
    ::operator delete[](ptr);
}

void* operator new(
    std::size_t size,
    std::align_val_t alignment)
{
    void* ptr = PlatformMemory::allocateAligned(
        size,
        static_cast<std::size_t>(alignment)
    );

    if (!ptr)
        throw std::bad_alloc();

    trackAllocation(ptr, size);

    return ptr;
}

void* operator new[](
    std::size_t size,
    std::align_val_t alignment)
{
    void* ptr = PlatformMemory::allocateAligned(
        size,
        static_cast<std::size_t>(alignment)
    );

    if (!ptr)
        throw std::bad_alloc();

    trackAllocation(ptr, size);

    return ptr;
}

void operator delete(
    void* ptr,
    std::align_val_t) noexcept
{
    trackDeallocation(ptr, 0);
    PlatformMemory::deallocateAligned(ptr);
}

void operator delete[](
    void* ptr,
    std::align_val_t) noexcept
{
    trackDeallocation(ptr, 0);
    PlatformMemory::deallocateAligned(ptr);
}

void operator delete(
    void* ptr,
    std::size_t size,
    std::align_val_t alignment) noexcept
{
    trackDeallocation(ptr, size);
    PlatformMemory::deallocateAligned(ptr);
}

void operator delete[](
    void* ptr,
    std::size_t size,
    std::align_val_t alignment) noexcept
{
    trackDeallocation(ptr, size);
    PlatformMemory::deallocateAligned(ptr);
}