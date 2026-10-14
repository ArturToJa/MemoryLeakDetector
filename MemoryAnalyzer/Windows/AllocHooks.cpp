// IAT-patching allocation hooks for Windows.
//
// What gets hooked:
//   * The UCRT's malloc/free/calloc/realloc/_aligned_malloc/_aligned_free
//     (plus the _dbg variants the debug CRT exports). operator new/delete
//     are NOT hooked separately: in every CRT flavour they bottom out in
//     these calls through some module's IAT, so hooking only the C
//     allocators sees each allocation exactly once (hooking both would
//     double count - the same hazard the Linux build avoids with its
//     reentrancy guards).
//   * kernel32's ExitProcess, so the leak report runs while the tracker
//     thread is still alive (DLL_PROCESS_DETACH runs too late for that).
//   * kernel32's LoadLibrary*, so modules loaded after injection are
//     patched too.
//
// Both the release (ucrtbase.dll) and debug (ucrtbased.dll) CRTs are
// handled, each with its own set of hooks, because a process can have both
// loaded (system DLLs use the release one, a debug build of the target the
// debug one) and a pointer must be freed by the CRT that allocated it.
//
// Matching is by *value*: any IAT slot holding the real function's address
// is patched, regardless of which DLL name (api-ms-win-crt-heap-..., or
// ucrtbase.dll directly) the import was declared against.
//
// Known limits: a target with a statically linked CRT has its own copy of
// malloc and is invisible to this; delay-loaded imports and
// GetProcAddress-resolved pointers are not patched.

#include "AllocHooks.h"
#include "../Session.h"
#include "Events.h"
#include "InterceptorGuard.h"
#include "Tracker.h"

#include <winternl.h>

#include <cstddef>

namespace
{
    HMODULE selfModule = nullptr;

    // `skipFrames` drops the hook's own frames from the captured trace so it
    // starts at the caller of malloc/realloc/etc. (best effort - inlining can
    // shift it by a frame).
    void trackAllocation(void* ptr, std::size_t size, int skipFrames = 2)
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
            PlatformStackTrace::capture(skipFrames)
        };

        Session::getEventQueue().push(event);
    }

    void trackDeallocation(void* ptr)
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
            0,
            {}
        };

        Session::getEventQueue().push(event);
    }

    void trackRealloc(void* old, void* fresh, std::size_t size)
    {
        if (old && fresh && fresh != old)
            trackDeallocation(old);

        // Same-pointer realloc: the tracker keys by address, so announcing
        // the allocation again simply replaces the entry with the new size.
        trackAllocation(fresh, size, 3);
    }

    struct Patch
    {
        void* real;
        void* hook;
    };

    constexpr int MaxPatches = 64;
    Patch patches[MaxPatches];
    int patchCount = 0;

    void addPatch(void* real, void* hook)
    {
        if (real && hook && patchCount < MaxPatches)
            patches[patchCount++] = {real, hook};
    }

    // One instantiation per CRT module (0 = ucrtbase, 1 = ucrtbased).
    template <int Crt>
    struct CrtHooks
    {
        using MallocFn = void* (__cdecl*)(std::size_t);
        using FreeFn = void (__cdecl*)(void*);
        using CallocFn = void* (__cdecl*)(std::size_t, std::size_t);
        using ReallocFn = void* (__cdecl*)(void*, std::size_t);
        using AlignedMallocFn = void* (__cdecl*)(std::size_t, std::size_t);
        using MallocDbgFn = void* (__cdecl*)(std::size_t, int, const char*, int);
        using FreeDbgFn = void (__cdecl*)(void*, int);
        using CallocDbgFn = void* (__cdecl*)(std::size_t, std::size_t, int, const char*, int);
        using ReallocDbgFn = void* (__cdecl*)(void*, std::size_t, int, const char*, int);

        static inline MallocFn realMalloc;
        static inline FreeFn realFree;
        static inline CallocFn realCalloc;
        static inline ReallocFn realRealloc;
        static inline AlignedMallocFn realAlignedMalloc;
        static inline FreeFn realAlignedFree;
        static inline MallocDbgFn realMallocDbg;
        static inline FreeDbgFn realFreeDbg;
        static inline CallocDbgFn realCallocDbg;
        static inline ReallocDbgFn realReallocDbg;

        static void* __cdecl hookMalloc(std::size_t size)
        {
            void* p = realMalloc(size);
            trackAllocation(p, size);
            return p;
        }

        static void __cdecl hookFree(void* p)
        {
            trackDeallocation(p);
            realFree(p);
        }

        static void* __cdecl hookCalloc(std::size_t count, std::size_t size)
        {
            void* p = realCalloc(count, size);
            trackAllocation(p, count * size);
            return p;
        }

        static void* __cdecl hookRealloc(void* old, std::size_t size)
        {
            void* p = realRealloc(old, size);
            trackRealloc(old, p, size);
            return p;
        }

        static void* __cdecl hookAlignedMalloc(std::size_t size, std::size_t alignment)
        {
            void* p = realAlignedMalloc(size, alignment);
            trackAllocation(p, size);
            return p;
        }

        static void __cdecl hookAlignedFree(void* p)
        {
            trackDeallocation(p);
            realAlignedFree(p);
        }

        static void* __cdecl hookMallocDbg(std::size_t size, int block, const char* file, int line)
        {
            void* p = realMallocDbg(size, block, file, line);
            trackAllocation(p, size);
            return p;
        }

        static void __cdecl hookFreeDbg(void* p, int block)
        {
            trackDeallocation(p);
            realFreeDbg(p, block);
        }

        static void* __cdecl hookCallocDbg(std::size_t count, std::size_t size, int block, const char* file, int line)
        {
            void* p = realCallocDbg(count, size, block, file, line);
            trackAllocation(p, count * size);
            return p;
        }

        static void* __cdecl hookReallocDbg(void* old, std::size_t size, int block, const char* file, int line)
        {
            void* p = realReallocDbg(old, size, block, file, line);
            trackRealloc(old, p, size);
            return p;
        }

        template <typename Fn>
        static void resolve(HMODULE crt, const char* name, Fn& real, void* hook)
        {
            real = reinterpret_cast<Fn>(GetProcAddress(crt, name));
            addPatch(reinterpret_cast<void*>(real), hook);
        }

        static void setup(const wchar_t* moduleName)
        {
            HMODULE crt = GetModuleHandleW(moduleName);

            if (!crt)
                return;

            resolve(crt, "malloc", realMalloc, reinterpret_cast<void*>(&hookMalloc));
            resolve(crt, "free", realFree, reinterpret_cast<void*>(&hookFree));
            resolve(crt, "calloc", realCalloc, reinterpret_cast<void*>(&hookCalloc));
            resolve(crt, "realloc", realRealloc, reinterpret_cast<void*>(&hookRealloc));
            resolve(crt, "_aligned_malloc", realAlignedMalloc, reinterpret_cast<void*>(&hookAlignedMalloc));
            resolve(crt, "_aligned_free", realAlignedFree, reinterpret_cast<void*>(&hookAlignedFree));
            resolve(crt, "_malloc_dbg", realMallocDbg, reinterpret_cast<void*>(&hookMallocDbg));
            resolve(crt, "_free_dbg", realFreeDbg, reinterpret_cast<void*>(&hookFreeDbg));
            resolve(crt, "_calloc_dbg", realCallocDbg, reinterpret_cast<void*>(&hookCallocDbg));
            resolve(crt, "_realloc_dbg", realReallocDbg, reinterpret_cast<void*>(&hookReallocDbg));
        }
    };

    // ---- kernel32 hooks -------------------------------------------------

    void patchAllModules();

    using ExitProcessFn = void (WINAPI*)(UINT);
    using LoadLibraryAFn = HMODULE (WINAPI*)(LPCSTR);
    using LoadLibraryWFn = HMODULE (WINAPI*)(LPCWSTR);
    using LoadLibraryExAFn = HMODULE (WINAPI*)(LPCSTR, HANDLE, DWORD);
    using LoadLibraryExWFn = HMODULE (WINAPI*)(LPCWSTR, HANDLE, DWORD);

    ExitProcessFn realExitProcess;
    LoadLibraryAFn realLoadLibraryA;
    LoadLibraryWFn realLoadLibraryW;
    LoadLibraryExAFn realLoadLibraryExA;
    LoadLibraryExWFn realLoadLibraryExW;

    void WINAPI hookExitProcess(UINT code)
    {
        {
            // Whatever the report itself allocates must not be tracked.
            InterceptorGuard guard;
            Session::shutdown();
        }

        realExitProcess(code);
    }

    HMODULE WINAPI hookLoadLibraryA(LPCSTR name)
    {
        HMODULE m = realLoadLibraryA(name);

        if (m)
            patchAllModules();

        return m;
    }

    HMODULE WINAPI hookLoadLibraryW(LPCWSTR name)
    {
        HMODULE m = realLoadLibraryW(name);

        if (m)
            patchAllModules();

        return m;
    }

    HMODULE WINAPI hookLoadLibraryExA(LPCSTR name, HANDLE file, DWORD flags)
    {
        HMODULE m = realLoadLibraryExA(name, file, flags);

        if (m)
            patchAllModules();

        return m;
    }

    HMODULE WINAPI hookLoadLibraryExW(LPCWSTR name, HANDLE file, DWORD flags)
    {
        HMODULE m = realLoadLibraryExW(name, file, flags);

        if (m)
            patchAllModules();

        return m;
    }

    // ---- IAT patching ---------------------------------------------------

    void patchModule(HMODULE module)
    {
        auto* base = reinterpret_cast<unsigned char*>(module);
        auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);

        if (dos->e_magic != IMAGE_DOS_SIGNATURE)
            return;

        auto* nt = reinterpret_cast<IMAGE_NT_HEADERS*>(base + dos->e_lfanew);

        if (nt->Signature != IMAGE_NT_SIGNATURE)
            return;

        const auto& dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];

        if (!dir.VirtualAddress)
            return;

        auto* desc = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(base + dir.VirtualAddress);

        for (; desc->Name; ++desc)
        {
            auto* thunk = reinterpret_cast<IMAGE_THUNK_DATA*>(base + desc->FirstThunk);

            for (; thunk->u1.Function; ++thunk)
            {
                void* current = reinterpret_cast<void*>(thunk->u1.Function);

                for (int i = 0; i < patchCount; ++i)
                {
                    if (current != patches[i].real)
                        continue;

                    DWORD oldProtect = 0;

                    if (VirtualProtect(&thunk->u1.Function, sizeof(void*), PAGE_READWRITE, &oldProtect))
                    {
                        thunk->u1.Function = reinterpret_cast<ULONG_PTR>(patches[i].hook);

                        DWORD ignored = 0;
                        VirtualProtect(&thunk->u1.Function, sizeof(void*), oldProtect, &ignored);
                    }

                    break;
                }
            }
        }
    }

    SRWLOCK patchLock = SRWLOCK_INIT;

    // Walks the loader's own module list (what GetModuleHandle consults), so
    // it is safe to call from DllMain with the loader lock held, unlike the
    // Toolhelp/PSAPI enumerators.
    void patchAllModules()
    {
        AcquireSRWLockExclusive(&patchLock);

        PEB* peb = NtCurrentTeb()->ProcessEnvironmentBlock;
        LIST_ENTRY* head = &peb->Ldr->InMemoryOrderModuleList;

        for (LIST_ENTRY* entry = head->Flink; entry != head; entry = entry->Flink)
        {
            auto* mod = CONTAINING_RECORD(entry, LDR_DATA_TABLE_ENTRY, InMemoryOrderLinks);
            auto module = static_cast<HMODULE>(mod->DllBase);

            if (module && module != selfModule)
                patchModule(module);
        }

        ReleaseSRWLockExclusive(&patchLock);
    }

    template <typename Fn>
    void resolveKernel(HMODULE kernel, const char* name, Fn& real, void* hook)
    {
        real = reinterpret_cast<Fn>(GetProcAddress(kernel, name));
        addPatch(reinterpret_cast<void*>(real), hook);
    }
}

namespace Hooks
{
    void install(HMODULE self)
    {
        selfModule = self;

        CrtHooks<0>::setup(L"ucrtbase.dll");
        CrtHooks<1>::setup(L"ucrtbased.dll");

        HMODULE kernel = GetModuleHandleW(L"kernel32.dll");

        resolveKernel(kernel, "ExitProcess", realExitProcess, reinterpret_cast<void*>(&hookExitProcess));
        resolveKernel(kernel, "LoadLibraryA", realLoadLibraryA, reinterpret_cast<void*>(&hookLoadLibraryA));
        resolveKernel(kernel, "LoadLibraryW", realLoadLibraryW, reinterpret_cast<void*>(&hookLoadLibraryW));
        resolveKernel(kernel, "LoadLibraryExA", realLoadLibraryExA, reinterpret_cast<void*>(&hookLoadLibraryExA));
        resolveKernel(kernel, "LoadLibraryExW", realLoadLibraryExW, reinterpret_cast<void*>(&hookLoadLibraryExW));

        patchAllModules();
    }
}
