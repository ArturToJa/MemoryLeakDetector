#include "AllocHooks.h"

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(module);
        Hooks::install(module);
    }

    // Deliberately nothing on DLL_PROCESS_DETACH: by then the process's
    // other threads (including the tracker thread) are already gone. The
    // leak report is produced from the ExitProcess hook instead - see
    // AllocHooks.cpp.
    return TRUE;
}
