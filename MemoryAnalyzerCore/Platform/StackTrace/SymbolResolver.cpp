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