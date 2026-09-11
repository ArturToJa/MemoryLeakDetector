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
    // Runtime's static initializer (see Runtime.cpp) tries to run as early as
    // the toolchain allows, but that's only ever a best-effort head start -
    // on some platforms/linkers (observed on Darwin, where neither
    // #pragma init_seg nor __attribute__((init_priority)) apply) a different
    // translation unit's global can still allocate before it. Rather than
    // lose that allocation, initialize on first use here instead of just
    // bailing out: this guarantees the very first operator new/delete call
    // anywhere in the process turns tracking on, regardless of which
    // translation unit's static initializer happened to run first.
    // Runtime::initialize() takes care of its own reentrancy guarding
    // internally (see Runtime.cpp) - it's not repeated here.
    //
    // Returns whether tracking is actually usable right now. Once
    // Runtime::shutdown() has run, EventQueue/Tracker/etc. are past the
    // point where anything should touch them again - they're ordinary
    // static-duration objects that get torn down like any other, on a
    // schedule this code has no control over, and a *different*
    // translation unit's global destructor (running later - cross-TU
    // destruction order is just as unspecified as construction order) can
    // still allocate or deallocate after they're gone. Calling
    // Runtime::getEventQueue().push() at that point would lock an
    // already-destroyed mutex - this is not hypothetical, it's the exact
    // "mutex lock failed" crash observed on macOS in CI. So once shutdown
    // has completed, the right answer is to drop the event, not queue it.
    bool ensureInitialized()
    {
        if (Runtime::isInitialized())
            return true;

        if (Runtime::isShutdownComplete())
            return false;

        Runtime::initialize();

        return Runtime::isInitialized();
    }

    void trackAllocation(void* ptr, std::size_t size)
    {
        if (!ptr)
            return;

        if (InterceptorGuard::isDisabled() || TrackingGuard::isTrackingDisabled())
            return;

        if (!ensureInitialized())
            return;

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

        if (InterceptorGuard::isDisabled() || TrackingGuard::isTrackingDisabled())
            return;

        if (!ensureInitialized())
            return;

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