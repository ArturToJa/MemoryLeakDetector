// macOS injection hooks: DYLD_INSERT_LIBRARIES + DYLD_INTERPOSE.
//
// Unlike Linux's LD_PRELOAD there is no symbol-name interposition to fight
// with here. dyld's interpose section rebinds every *other* image's
// references to the original function to our replacement, and leaves this
// image's own references alone - so inside a replacement, calling plain
// malloc()/free() reaches the real implementation. That removes the whole
// dlsym(RTLD_NEXT)/bootstrap-buffer dance the Linux build needs.
//
// operator new/delete are deliberately NOT interposed: libc++abi's
// operator new calls malloc through a stub, which dyld rebinds to the
// replacement below, so every C++ allocation is seen exactly once.
// Interposing both would double count.
//
// Reentrancy: the guards in the Engine (InterceptorGuard/TrackingGuard) are
// thread_local, and on macOS the first access to a thread_local on a thread
// allocates its storage through malloc - which, interposed, would re-enter
// these hooks while that very access is still in flight. So the hooks use
// their own pthread-key-based flag (plain TSD slot, no allocation) as an
// outer guard: while it is set, nothing in the Engine's thread_locals is
// touched on re-entry, and any nested malloc just falls straight through.

#include "../Session.h"
#include "Events.h"
#include "InterceptorGuard.h"
#include "Tracker.h"

#include <atomic>
#include <cstddef>
#include <cstdlib>

#include <pthread.h>

namespace
{
    pthread_key_t guardKey;
    std::atomic<bool> guardReady{false};

    // Marks this thread as "already inside a hook". Does nothing (and
    // reports inactive) if the key isn't created yet or we're re-entering.
    class Reentry
    {
    public:
        Reentry()
        {
            if (guardReady.load(std::memory_order_acquire) &&
                !pthread_getspecific(guardKey))
            {
                pthread_setspecific(guardKey, reinterpret_cast<void*>(1));
                entered = true;
            }
        }

        ~Reentry()
        {
            if (entered)
                pthread_setspecific(guardKey, nullptr);
        }

        Reentry(const Reentry&) = delete;
        Reentry& operator=(const Reentry&) = delete;

        bool active() const { return entered; }

    private:
        bool entered = false;
    };

    // Runs at load time, after libSystem and libc++ are up (they are our
    // dependencies) but before the target's main(). Allocations made before
    // this simply go untracked.
    __attribute__((constructor)) void initGuard()
    {
        if (pthread_key_create(&guardKey, nullptr) == 0)
            guardReady.store(true, std::memory_order_release);
    }

    void trackAllocation(void* ptr, std::size_t size)
    {
        if (!ptr)
            return;

        Reentry reentry;

        if (!reentry.active())
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
            PlatformStackTrace::capture(2)
        };

        Session::getEventQueue().push(event);
    }

    void trackDeallocation(void* ptr)
    {
        if (!ptr)
            return;

        Reentry reentry;

        if (!reentry.active())
            return;

        if (InterceptorGuard::isDisabled() || TrackingGuard::isTrackingDisabled())
            return;

        if (!Session::ensureInitialized())
            return;

        InterceptorGuard guard;

        AllocationEvent event{
            EventType::Deallocate,
            ptr,
            0,
            {}
        };

        Session::getEventQueue().push(event);
    }

    void* hookMalloc(std::size_t size)
    {
        void* ptr = malloc(size);
        trackAllocation(ptr, size);
        return ptr;
    }

    void hookFree(void* ptr)
    {
        trackDeallocation(ptr);
        free(ptr);
    }

    void* hookCalloc(std::size_t count, std::size_t size)
    {
        void* ptr = calloc(count, size);
        trackAllocation(ptr, count * size);
        return ptr;
    }

    void* hookRealloc(void* old, std::size_t size)
    {
        void* fresh = realloc(old, size);

        if (old && fresh && fresh != old)
            trackDeallocation(old);

        // Same-pointer realloc: the tracker keys by address, so announcing
        // the allocation again replaces the entry with the new size.
        trackAllocation(fresh, size);

        return fresh;
    }

    int hookPosixMemalign(void** out, std::size_t alignment, std::size_t size)
    {
        int result = posix_memalign(out, alignment, size);

        if (result == 0)
            trackAllocation(*out, size);

        return result;
    }

    void* hookAlignedAlloc(std::size_t alignment, std::size_t size)
    {
        void* ptr = aligned_alloc(alignment, size);
        trackAllocation(ptr, size);
        return ptr;
    }

    void* hookValloc(std::size_t size)
    {
        void* ptr = valloc(size);
        trackAllocation(ptr, size);
        return ptr;
    }
}

// From <mach-o/dyld-interposing.h>, spelled out here because that header
// isn't part of the public SDK. The C-style casts are deliberate: unlike
// reinterpret_cast they keep the initializer a link-time constant, which
// dyld needs because it reads this section straight out of the image,
// before any C++ static initializer has run.
#define DYLD_INTERPOSE(replacement, replacee)                                   \
    __attribute__((used)) static struct                                        \
    {                                                                          \
        const void* replacementFn;                                             \
        const void* replaceeFn;                                                \
    } interpose_##replacee __attribute__((section("__DATA,__interpose"))) = {  \
        (const void*)(unsigned long)&replacement,                              \
        (const void*)(unsigned long)&replacee                                  \
    }

DYLD_INTERPOSE(hookMalloc, malloc);
DYLD_INTERPOSE(hookFree, free);
DYLD_INTERPOSE(hookCalloc, calloc);
DYLD_INTERPOSE(hookRealloc, realloc);
DYLD_INTERPOSE(hookPosixMemalign, posix_memalign);
DYLD_INTERPOSE(hookAlignedAlloc, aligned_alloc);
DYLD_INTERPOSE(hookValloc, valloc);
