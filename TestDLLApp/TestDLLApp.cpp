// Demo/target for the Windows injection library, meant to be run straight
// from the IDE (F5 / Ctrl+F5 on this target).
//
// It has two modes, picked by whether MemoryAnalyzer.dll is already loaded
// in this process:
//
//   * Not injected (you just ran it): re-launch *itself* through
//     MemoryAnalyzerLauncher, which starts a fresh copy suspended and
//     injects the DLL into it, wait for that to finish, then print the leak
//     log. Visual Studio's CMake folder mode can only run a target's own
//     executable, so this is how "run the injector with this app" is made
//     the default action.
//
//   * Injected (the launcher started us): run the deliberately leaky code.
//
// The paths to the launcher and the DLL are baked in by CMake
// (see CMakeLists.txt), so both are guaranteed to be built first.

#include <windows.h>

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#ifndef LAUNCHER_PATH
#error LAUNCHER_PATH must be defined by the build
#endif

#ifndef ANALYZER_DLL_PATH
#error ANALYZER_DLL_PATH must be defined by the build
#endif

namespace
{
    int runLeakyCode()
    {
        std::printf("[TestDLLApp] running under the injector\n");

        // Freed - must NOT show up in the report.
        void* freed = std::malloc(1001);
        std::free(freed);

        char* freedArray = new char[1003];
        delete[] freedArray;

        // Never freed - each should be reported with its stack trace.
        void* leakedMalloc = std::malloc(2001);
        (void)leakedMalloc;

        char* leakedArray = new char[2003];
        (void)leakedArray;

        auto* leakedVector = new std::vector<int>(500);
        (void)leakedVector; // 500 ints + the vector object itself

        void* grown = std::malloc(16);
        grown = std::realloc(grown, 2005);
        (void)grown;

        std::printf("[TestDLLApp] done; expect leaks of 2001, 2003, 2005 bytes "
                    "(+ 2000 and the vector object)\n");

        return 0;
    }

    std::string selfPath()
    {
        char path[MAX_PATH];
        GetModuleFileNameA(nullptr, path, MAX_PATH);
        return path;
    }

    int relaunchUnderInjector()
    {
        const std::string self = selfPath();

        // Same folder as this exe, unless the caller already chose a path.
        if (!std::getenv("MEMANALYZER_LOG_PATH"))
        {
            std::string logPath = self.substr(0, self.find_last_of("\\/") + 1) + "TestDLLApp-leaks.log";
            _putenv_s("MEMANALYZER_LOG_PATH", logPath.c_str());
        }

        const std::string logPath = std::getenv("MEMANALYZER_LOG_PATH");

        std::string commandLine =
            std::string("\"") + LAUNCHER_PATH + "\" \"" + ANALYZER_DLL_PATH + "\" \"" + self + "\"";

        std::printf("[TestDLLApp] launching through the injector:\n  %s\n\n", commandLine.c_str());
        std::fflush(stdout);

        STARTUPINFOA startup{};
        startup.cb = sizeof(startup);
        PROCESS_INFORMATION info{};

        if (!CreateProcessA(nullptr, commandLine.data(), nullptr, nullptr, TRUE, 0,
                            nullptr, nullptr, &startup, &info))
        {
            std::fprintf(stderr, "[TestDLLApp] could not start the launcher (error %lu)\n", GetLastError());
            return 1;
        }

        WaitForSingleObject(info.hProcess, INFINITE);

        DWORD exitCode = 0;
        GetExitCodeProcess(info.hProcess, &exitCode);

        CloseHandle(info.hThread);
        CloseHandle(info.hProcess);

        std::printf("\n[TestDLLApp] injected run exited with code %lu; leak log (%s):\n"
                    "----------------------------------------------------------\n",
                    exitCode, logPath.c_str());

        std::ifstream log(logPath);

        if (!log)
        {
            std::printf("(no log file was written - the DLL probably failed to inject)\n");
            return exitCode ? static_cast<int>(exitCode) : 1;
        }

        std::cout << log.rdbuf();

        return static_cast<int>(exitCode);
    }
}

int main()
{
    if (GetModuleHandleW(L"MemoryAnalyzer.dll"))
        return runLeakyCode();

    return relaunchUnderInjector();
}
