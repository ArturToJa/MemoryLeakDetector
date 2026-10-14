#pragma once

#include <windows.h>

// The Windows counterpart of Linux/MallocHooks.cpp - but where LD_PRELOAD
// gets interposition from the dynamic linker for free, Windows has no
// equivalent, so hooking is done by hand: every loaded module's import
// address table (IAT) is scanned and any slot pointing at one of the real
// functions we care about is overwritten with a pointer to our hook. See
// AllocHooks.cpp for what is hooked and why.
namespace Hooks
{
    // Resolves the real functions and patches every module currently loaded
    // in the process (except `self`, so the library's own allocations
    // always reach the real CRT unobserved). Also hooks LoadLibrary* so
    // modules loaded later get patched too. Safe to call from DllMain.
    void install(HMODULE self);
}
