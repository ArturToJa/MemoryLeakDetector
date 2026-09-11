#include "SymbolResolver.h"

#ifdef _WIN32

#include <windows.h>
#include <dbghelp.h>
#include <cstdlib>

namespace
{
    constexpr std::size_t SymbolBufferSize =
        sizeof(SYMBOL_INFO) + 256;
}

SymbolResolver& SymbolResolver::instance()
{
    static SymbolResolver resolver;
    return resolver;
}

bool SymbolResolver::initialize()
{
    std::lock_guard<std::mutex> lock(mutex);

    if (initialized)
        return true;

    HANDLE process = GetCurrentProcess();

    SymSetOptions(
        SYMOPT_UNDNAME |
        SYMOPT_DEFERRED_LOADS
    );

    if (!SymInitialize(process, nullptr, TRUE))
    {
        return false;
    }

    initialized = true;

    return true;
}

void SymbolResolver::shutdown()
{
    std::lock_guard<std::mutex> lock(mutex);

    if (!initialized)
        return;

    HANDLE process = GetCurrentProcess();

    SymCleanup(process);

    initialized = false;
}

bool SymbolResolver::resolve(
    std::uintptr_t address,
    std::string& function,
    std::string& file,
    std::uint32_t& line)
{
    function.clear();
    file.clear();
    line = 0;

    std::lock_guard<std::mutex> lock(mutex);

    if (!initialized)
        return false;

    HANDLE process = GetCurrentProcess();

    auto* symbolBuffer =
        static_cast<SYMBOL_INFO*>(
            std::calloc(1, SymbolBufferSize)
            );

    if (!symbolBuffer)
        return false;

    symbolBuffer->SizeOfStruct = sizeof(SYMBOL_INFO);
    symbolBuffer->MaxNameLen = 255;

    DWORD64 displacement = 0;

    bool resolved = false;

    if (SymFromAddr(
        process,
        static_cast<DWORD64>(address),
        &displacement,
        symbolBuffer))
    {
        function = symbolBuffer->Name;
        resolved = true;

        IMAGEHLP_LINE64 lineInfo{};
        lineInfo.SizeOfStruct = sizeof(IMAGEHLP_LINE64);

        DWORD lineDisplacement = 0;

        if (SymGetLineFromAddr64(
            process,
            static_cast<DWORD64>(address),
            &lineDisplacement,
            &lineInfo))
        {
            file = lineInfo.FileName;
            line = lineInfo.LineNumber;
        }
    }

    std::free(symbolBuffer);

    return resolved;
}

#elif defined(__linux__)

// Resolves a runtime address to a function name and file:line by shelling
// out to addr2line - no extra library dependency beyond binutils, which is
// already required to build anything. This only runs at leak-report time
// (once per outstanding leak's stack frames, at process shutdown), so the
// cost of spawning a process per frame is not a concern.
//
// dladdr() gives us both the containing module's path and its load base;
// subtracting the base from the runtime address yields the link-time
// address addr2line expects (correct for both PIE and non-PIE binaries -
// for a non-PIE executable the base is simply the address the link-time
// addresses already assume, so the subtraction is a no-op in practice).
//
// fork/exec with an explicit argv (not popen/system) is used deliberately
// so the module path never passes through a shell - it comes from the
// loader's own module table rather than untrusted input, but there is no
// reason to introduce a shell-quoting concern here when argv-based exec
// avoids it entirely.

#include <dlfcn.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>

#include <cinttypes>
#include <cstdio>

SymbolResolver& SymbolResolver::instance()
{
    static SymbolResolver resolver;
    return resolver;
}

bool SymbolResolver::initialize()
{
    std::lock_guard<std::mutex> lock(mutex);

    initialized = true;

    return true;
}

void SymbolResolver::shutdown()
{
    std::lock_guard<std::mutex> lock(mutex);

    initialized = false;
}

bool SymbolResolver::resolve(
    std::uintptr_t address,
    std::string& function,
    std::string& file,
    std::uint32_t& line)
{
    function.clear();
    file.clear();
    line = 0;

    std::lock_guard<std::mutex> lock(mutex);

    if (!initialized)
        return false;

    Dl_info info{};

    if (!dladdr(reinterpret_cast<void*>(address), &info) || !info.dli_fname)
        return false;

    std::uintptr_t offset =
        address - reinterpret_cast<std::uintptr_t>(info.dli_fbase);

    char offsetArg[32];
    std::snprintf(offsetArg, sizeof(offsetArg), "0x%" PRIxPTR, offset);

    int outPipe[2];

    if (pipe(outPipe) != 0)
        return false;

    pid_t pid = fork();

    if (pid < 0)
    {
        close(outPipe[0]);
        close(outPipe[1]);

        return false;
    }

    if (pid == 0)
    {
        dup2(outPipe[1], STDOUT_FILENO);
        close(outPipe[0]);
        close(outPipe[1]);

        int devNull = open("/dev/null", O_WRONLY);

        if (devNull >= 0)
        {
            dup2(devNull, STDERR_FILENO);
            close(devNull);
        }

        execlp(
            "addr2line", "addr2line",
            "-f", "-C",
            "-e", info.dli_fname,
            offsetArg,
            static_cast<char*>(nullptr)
        );

        _exit(127);
    }

    close(outPipe[1]);

    std::string output;
    char buffer[256];
    ssize_t bytesRead = 0;

    while ((bytesRead = read(outPipe[0], buffer, sizeof(buffer))) > 0)
    {
        output.append(buffer, static_cast<std::size_t>(bytesRead));
    }

    close(outPipe[0]);

    int status = 0;
    waitpid(pid, &status, 0);

    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
        return false;

    auto firstNewline = output.find('\n');

    if (firstNewline == std::string::npos)
        return false;

    std::string parsedFunction = output.substr(0, firstNewline);

    std::string rest = output.substr(firstNewline + 1);
    auto secondNewline = rest.find('\n');

    std::string fileLine =
        (secondNewline == std::string::npos) ? rest : rest.substr(0, secondNewline);

    auto colonPos = fileLine.rfind(':');

    if (colonPos != std::string::npos)
    {
        std::string parsedFile = fileLine.substr(0, colonPos);
        std::string parsedLineStr = fileLine.substr(colonPos + 1);

        if (parsedFile != "??")
            file = parsedFile;

        try
        {
            line = static_cast<std::uint32_t>(std::stoul(parsedLineStr));
        }
        catch (...)
        {
            line = 0;
        }
    }

    if (parsedFunction != "??")
        function = parsedFunction;

    return !function.empty() || !file.empty();
}

#elif defined(__APPLE__)

// macOS has no addr2line; the equivalent bundled with Xcode's command line
// tools is atos. Unlike addr2line, atos wants the *raw* runtime address
// plus the module's load address (via -l) and does the offset math itself,
// so - unlike the Linux branch - dli_fbase is passed through rather than
// subtracted here. atos's resolved output looks like one of:
//   functionName (in ModuleName) (file.cpp:123)   - symbols + debug info
//   functionName (in ModuleName)                  - symbols, no line info
//   0x1023456ab                                   - nothing resolved
//
// Same fork/exec-with-argv approach as the Linux branch, for the same
// reason: no shell, so no quoting concerns around the module path.

#include <dlfcn.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>

#include <cinttypes>
#include <cstdio>

SymbolResolver& SymbolResolver::instance()
{
    static SymbolResolver resolver;
    return resolver;
}

bool SymbolResolver::initialize()
{
    std::lock_guard<std::mutex> lock(mutex);

    initialized = true;

    return true;
}

void SymbolResolver::shutdown()
{
    std::lock_guard<std::mutex> lock(mutex);

    initialized = false;
}

bool SymbolResolver::resolve(
    std::uintptr_t address,
    std::string& function,
    std::string& file,
    std::uint32_t& line)
{
    function.clear();
    file.clear();
    line = 0;

    std::lock_guard<std::mutex> lock(mutex);

    if (!initialized)
        return false;

    Dl_info info{};

    if (!dladdr(reinterpret_cast<void*>(address), &info) || !info.dli_fname)
        return false;

    char loadAddrArg[32];
    std::snprintf(
        loadAddrArg, sizeof(loadAddrArg),
        "0x%" PRIxPTR,
        reinterpret_cast<std::uintptr_t>(info.dli_fbase)
    );

    char addressArg[32];
    std::snprintf(addressArg, sizeof(addressArg), "0x%" PRIxPTR, address);

    int outPipe[2];

    if (pipe(outPipe) != 0)
        return false;

    pid_t pid = fork();

    if (pid < 0)
    {
        close(outPipe[0]);
        close(outPipe[1]);

        return false;
    }

    if (pid == 0)
    {
        dup2(outPipe[1], STDOUT_FILENO);
        close(outPipe[0]);
        close(outPipe[1]);

        int devNull = open("/dev/null", O_WRONLY);

        if (devNull >= 0)
        {
            dup2(devNull, STDERR_FILENO);
            close(devNull);
        }

        execlp(
            "atos", "atos",
            "-o", info.dli_fname,
            "-l", loadAddrArg,
            addressArg,
            static_cast<char*>(nullptr)
        );

        _exit(127);
    }

    close(outPipe[1]);

    std::string output;
    char buffer[256];
    ssize_t bytesRead = 0;

    while ((bytesRead = read(outPipe[0], buffer, sizeof(buffer))) > 0)
    {
        output.append(buffer, static_cast<std::size_t>(bytesRead));
    }

    close(outPipe[0]);

    int status = 0;
    waitpid(pid, &status, 0);

    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
        return false;

    while (!output.empty() && (output.back() == '\n' || output.back() == '\r'))
        output.pop_back();

    auto inMarker = output.find(" (in ");

    if (inMarker == std::string::npos)
        return false; // atos couldn't resolve anything - it just echoed the address back.

    function = output.substr(0, inMarker);

    auto moduleParenEnd = output.find(')', inMarker);

    if (moduleParenEnd != std::string::npos)
    {
        auto fileParenStart = output.find('(', moduleParenEnd);

        if (fileParenStart != std::string::npos)
        {
            auto fileParenEnd = output.find(')', fileParenStart);

            if (fileParenEnd != std::string::npos)
            {
                std::string fileLine = output.substr(
                    fileParenStart + 1,
                    fileParenEnd - fileParenStart - 1
                );

                auto colonPos = fileLine.rfind(':');

                if (colonPos != std::string::npos)
                {
                    file = fileLine.substr(0, colonPos);

                    try
                    {
                        line = static_cast<std::uint32_t>(
                            std::stoul(fileLine.substr(colonPos + 1))
                        );
                    }
                    catch (...)
                    {
                        line = 0;
                    }
                }
            }
        }
    }

    return !function.empty();
}

#else

SymbolResolver& SymbolResolver::instance()
{
    static SymbolResolver resolver;
    return resolver;
}

bool SymbolResolver::initialize()
{
    return true;
}

void SymbolResolver::shutdown()
{
}

bool SymbolResolver::resolve(
    std::uintptr_t,
    std::string& function,
    std::string& file,
    std::uint32_t& line)
{
    function.clear();
    file.clear();
    line = 0;

    return false;
}

#endif