// The injected-library counterpart to MemoryAnalyzerCore's NewDelete.cpp:
// the same operator new/delete override set (plain, array, nothrow,
// aligned), but routing the actual allocation work through RealAllocator
// (dlsym-resolved real functions) instead of calling malloc/posix_memalign
// directly by name - see Linux/RealAllocator.h for why that distinction
// matters under LD_PRELOAD.

#include "Session.h"
#include "Events.h"
#include "InterceptorGuard.h"
#include "Tracker.h"
#include "Linux/RealAllocator.h"

#include <cstddef>
#include <new>

namespace
{
    void trackAllocation(void* ptr, std::size_t size)
    {
        if (!ptr)
            return;

        if (InterceptorGuard::isDisabled() || TrackingGuard::isTrackingDisabled())
            return;

        if (!Session::ensureInitialized())
            return;

        InterceptorGuard guard;

        AllocationEvent event{
            EventType::Allocate,
            ptr,
            size,
            PlatformStackTrace::capture(1)
        };

        Session::getEventQueue().push(event);
    }

    void trackDeallocation(void* ptr, std::size_t size)
    {
        if (!ptr)
            return;

        if (InterceptorGuard::isDisabled() || TrackingGuard::isTrackingDisabled())
            return;

        if (!Session::ensureInitialized())
            return;

        InterceptorGuard guard;

        AllocationEvent event{
            EventType::Deallocate,
            ptr,
            size,
            {}
        };

        Session::getEventQueue().push(event);
    }
}

void* operator new(std::size_t size)
{
    void* ptr = RealAllocator::malloc(size);

    if (!ptr)
        throw std::bad_alloc();

    trackAllocation(ptr, size);

    return ptr;
}

void* operator new[](std::size_t size)
{
    void* ptr = RealAllocator::malloc(size);

    if (!ptr)
        throw std::bad_alloc();

    trackAllocation(ptr, size);

    return ptr;
}

void operator delete(void* ptr) noexcept
{
    trackDeallocation(ptr, 0);
    RealAllocator::free(ptr);
}

void operator delete[](void* ptr) noexcept
{
    trackDeallocation(ptr, 0);
    RealAllocator::free(ptr);
}

void operator delete(void* ptr, std::size_t size) noexcept
{
    trackDeallocation(ptr, size);
    RealAllocator::free(ptr);
}

void operator delete[](void* ptr, std::size_t size) noexcept
{
    trackDeallocation(ptr, size);
    RealAllocator::free(ptr);
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
    void* ptr = RealAllocator::alignedAlloc(
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
    void* ptr = RealAllocator::alignedAlloc(
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
    RealAllocator::free(ptr);
}

void operator delete[](
    void* ptr,
    std::align_val_t) noexcept
{
    trackDeallocation(ptr, 0);
    RealAllocator::free(ptr);
}

void operator delete(
    void* ptr,
    std::size_t size,
    std::align_val_t) noexcept
{
    trackDeallocation(ptr, size);
    RealAllocator::free(ptr);
}

void operator delete[](
    void* ptr,
    std::size_t size,
    std::align_val_t) noexcept
{
    trackDeallocation(ptr, size);
    RealAllocator::free(ptr);
}
