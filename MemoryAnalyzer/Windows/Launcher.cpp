// MemoryAnalyzerLauncher <MemoryAnalyzer.dll> <target.exe> [target args...]
//
// Starts the target suspended, injects the analyzer DLL into it with a
// remote LoadLibraryW thread (whose DllMain installs the hooks), then
// resumes it - so the hooks are in place before the target's own code (even
// its CRT startup) runs. Standard handles are inherited, and the target's
// exit code is forwarded as the launcher's own.

#include <windows.h>

#include <cstdio>
#include <string>

namespace
{
    // Standard CommandLineToArgvW-compatible quoting.
    void appendQuoted(std::wstring& out, const std::wstring& arg)
    {
        if (!arg.empty() && arg.find_first_of(L" \t\n\v\"") == std::wstring::npos)
        {
            out += arg;
            return;
        }

        out += L'"';

        for (std::size_t i = 0;; ++i)
        {
            std::size_t backslashes = 0;

            while (i < arg.size() && arg[i] == L'\\')
            {
                ++backslashes;
                ++i;
            }

            if (i == arg.size())
            {
                out.append(backslashes * 2, L'\\');
                break;
            }

            if (arg[i] == L'"')
            {
                out.append(backslashes * 2 + 1, L'\\');
                out += L'"';
            }
            else
            {
                out.append(backslashes, L'\\');
                out += arg[i];
            }
        }

        out += L'"';
    }

    bool injectDll(HANDLE process, const std::wstring& dllPath)
    {
        const std::size_t bytes = (dllPath.size() + 1) * sizeof(wchar_t);

        void* remote = VirtualAllocEx(process, nullptr, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

        if (!remote)
            return false;

        bool ok = false;

        if (WriteProcessMemory(process, remote, dllPath.c_str(), bytes, nullptr))
        {
            // kernel32 is mapped at the same address in every process of a
            // boot session, so our own LoadLibraryW address is valid there.
            auto loadLibrary = reinterpret_cast<LPTHREAD_START_ROUTINE>(
                GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "LoadLibraryW"));

            HANDLE thread = CreateRemoteThread(process, nullptr, 0, loadLibrary, remote, 0, nullptr);

            if (thread)
            {
                WaitForSingleObject(thread, INFINITE);

                DWORD result = 0;
                GetExitCodeThread(thread, &result);

                // LoadLibraryW's return value (truncated to 32 bits): zero
                // means the load failed.
                ok = result != 0;

                CloseHandle(thread);
            }
        }

        VirtualFreeEx(process, remote, 0, MEM_RELEASE);

        return ok;
    }
}

int wmain(int argc, wchar_t** argv)
{
    if (argc < 3)
    {
        std::fprintf(stderr, "usage: MemoryAnalyzerLauncher <MemoryAnalyzer.dll> <target.exe> [args...]\n");
        return 2;
    }

    wchar_t fullDll[MAX_PATH];

    if (!GetFullPathNameW(argv[1], MAX_PATH, fullDll, nullptr))
    {
        std::fprintf(stderr, "MemoryAnalyzerLauncher: bad DLL path\n");
        return 2;
    }

    std::wstring commandLine;

    for (int i = 2; i < argc; ++i)
    {
        if (i > 2)
            commandLine += L' ';

        appendQuoted(commandLine, argv[i]);
    }

    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION info{};

    if (!CreateProcessW(argv[2], commandLine.data(), nullptr, nullptr, TRUE,
                        CREATE_SUSPENDED, nullptr, nullptr, &startup, &info))
    {
        std::fprintf(stderr, "MemoryAnalyzerLauncher: CreateProcess failed (%lu)\n", GetLastError());
        return 3;
    }

    if (!injectDll(info.hProcess, fullDll))
    {
        std::fprintf(stderr, "MemoryAnalyzerLauncher: DLL injection failed\n");
        TerminateProcess(info.hProcess, 4);
        CloseHandle(info.hThread);
        CloseHandle(info.hProcess);
        return 4;
    }

    ResumeThread(info.hThread);
    WaitForSingleObject(info.hProcess, INFINITE);

    DWORD exitCode = 0;
    GetExitCodeProcess(info.hProcess, &exitCode);

    CloseHandle(info.hThread);
    CloseHandle(info.hProcess);

    return static_cast<int>(exitCode);
}
