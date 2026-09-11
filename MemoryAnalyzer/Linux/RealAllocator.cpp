#include "RealAllocator.h"

#include <atomic>
#include <cstring>

#include <dlfcn.h>

namespace
{
    using MallocFn = void* (*)(std::size_t);
    using FreeFn = void (*)(void*);
    using CallocFn = void* (*)(std::size_t, std::size_t);
    using ReallocFn = void* (*)(void*, std::size_t);
    using PosixMemalignFn = int (*)(void**, std::size_t, std::size_t);

    std::atomic<MallocFn> realMalloc{nullptr};
    std::atomic<FreeFn> realFree{nullptr};
    std::atomic<CallocFn> realCalloc{nullptr};
    std::atomic<ReallocFn> realRealloc{nullptr};
    std::atomic<PosixMemalignFn> realPosixMemalign{nullptr};

    // A tiny bump allocator used only to satisfy calloc calls that happen
    // while we are still resolving the real calloc via dlsym. dlsym can
    // itself call calloc internally on some glibc versions (to allocate a
    // per-thread error-message buffer, the first time any dl* function is
    // used on that thread) - calling dlsym(RTLD_NEXT, "calloc") again from
    // inside that nested call would recurse forever, since our own calloc
    // is what gets called process-wide once LD_PRELOAD is in effect.
    // Serving it from a static buffer instead breaks the cycle. It's never
    // actually freed for real (free()/realloc() below just leave it alone
    // for pointers in this range), which is fine given how small and
    // short-lived this bootstrap window is.
    constexpr std::size_t BootstrapBufferSize = 4096;
    alignas(std::max_align_t) unsigned char bootstrapBuffer[BootstrapBufferSize];
    std::atomic<std::size_t> bootstrapOffset{0};
    thread_local bool resolvingCalloc = false;

    void* bootstrapCalloc(std::size_t total)
    {
        std::size_t offset = bootstrapOffset.fetch_add(total, std::memory_order_relaxed);

        if (offset + total > BootstrapBufferSize)
            return nullptr;

        void* ptr = bootstrapBuffer + offset;
        std::memset(ptr, 0, total);

        return ptr;
    }
}

namespace RealAllocator
{
    void* malloc(std::size_t size)
    {
        MallocFn fn = realMalloc.load(std::memory_order_acquire);

        if (!fn)
        {
            fn = reinterpret_cast<MallocFn>(dlsym(RTLD_NEXT, "malloc"));
            realMalloc.store(fn, std::memory_order_release);
        }

        return fn ? fn(size) : nullptr;
    }

    void free(void* ptr)
    {
        if (!ptr || isInternalBootstrapPointer(ptr))
            return;

        FreeFn fn = realFree.load(std::memory_order_acquire);

        if (!fn)
        {
            fn = reinterpret_cast<FreeFn>(dlsym(RTLD_NEXT, "free"));
            realFree.store(fn, std::memory_order_release);
        }

        if (fn)
            fn(ptr);
    }

    void* calloc(std::size_t count, std::size_t size)
    {
        CallocFn fn = realCalloc.load(std::memory_order_acquire);

        if (!fn)
        {
            if (resolvingCalloc)
                return bootstrapCalloc(count * size);

            resolvingCalloc = true;
            fn = reinterpret_cast<CallocFn>(dlsym(RTLD_NEXT, "calloc"));
            resolvingCalloc = false;

            if (!fn)
                return bootstrapCalloc(count * size);

            realCalloc.store(fn, std::memory_order_release);
        }

        return fn(count, size);
    }

    void* realloc(void* ptr, std::size_t size)
    {
        if (isInternalBootstrapPointer(ptr))
        {
            // Can't grow a bump-allocated block in place; the bootstrap
            // buffer only ever serves dlsym's own tiny internal
            // allocation, which nothing legitimately reallocs, so a
            // best-effort fresh allocation is fine here.
            return RealAllocator::malloc(size);
        }

        ReallocFn fn = realRealloc.load(std::memory_order_acquire);

        if (!fn)
        {
            fn = reinterpret_cast<ReallocFn>(dlsym(RTLD_NEXT, "realloc"));
            realRealloc.store(fn, std::memory_order_release);
        }

        return fn ? fn(ptr, size) : nullptr;
    }

    void* alignedAlloc(std::size_t size, std::size_t alignment)
    {
        PosixMemalignFn fn = realPosixMemalign.load(std::memory_order_acquire);

        if (!fn)
        {
            fn = reinterpret_cast<PosixMemalignFn>(dlsym(RTLD_NEXT, "posix_memalign"));
            realPosixMemalign.store(fn, std::memory_order_release);
        }

        if (!fn)
            return nullptr;

        if (alignment < sizeof(void*))
            alignment = sizeof(void*);

        void* ptr = nullptr;

        if (fn(&ptr, alignment, size) != 0)
            return nullptr;

        return ptr;
    }

    bool isInternalBootstrapPointer(void* ptr)
    {
        auto* p = static_cast<unsigned char*>(ptr);
        return p >= bootstrapBuffer && p < bootstrapBuffer + BootstrapBufferSize;
    }
}
