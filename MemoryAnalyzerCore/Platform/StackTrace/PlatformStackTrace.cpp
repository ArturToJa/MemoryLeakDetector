#include "PlatformStackTrace.h"
#include "SymbolResolver.h"

#ifdef _WIN32

#include <windows.h>
#include <dbghelp.h>
#include <cstdlib>
#include <utility>

namespace PlatformStackTrace
{
    StackTrace capture(std::size_t skipFrames)
    {
        void* frames[kMaxStackFrames];

        USHORT count = CaptureStackBackTrace(
            static_cast<DWORD>(skipFrames + 1),
            static_cast<DWORD>(kMaxStackFrames),
            frames,
            nullptr
        );

        StackTrace trace;
        trace.reserve(count);

        for (USHORT i = 0; i < count; ++i)
        {
            trace.push_back(
                reinterpret_cast<std::uintptr_t>(frames[i])
            );
        }

        return trace;
    }

    SymbolizedStackTrace symbolize(const StackTrace& trace)
    {
        SymbolizedStackTrace result;

        auto& resolver = SymbolResolver::instance();

        for (std::uintptr_t address : trace)
        {
            SymbolizedFrame frame{};
            frame.address = address;

            resolver.resolve(
                address,
                frame.function,
                frame.file,
                frame.line
            );

            result.push_back(std::move(frame));
        }

        return result;
    }

    std::string format(const SymbolizedStackTrace& trace)
    {
        std::string result;

        for (const auto& frame : trace)
        {
            if (!frame.function.empty())
            {
                result += frame.function;
            }
            else
            {
                char buffer[64];

                sprintf_s(
                    buffer,
                    "0x%llX",
                    static_cast<unsigned long long>(frame.address)
                );

                result += buffer;
            }

            if (!frame.file.empty())
            {
                result += " - ";
                result += frame.file;
                result += ":";

                result += std::to_string(frame.line);
            }

            result += "\n";
        }

        return result;
    }
}

#elif defined(__linux__)

#include <execinfo.h>
#include <cstdio>
#include <utility>

namespace PlatformStackTrace
{
    StackTrace capture(std::size_t skipFrames)
    {
        void* frames[kMaxStackFrames];

        int count = ::backtrace(frames, static_cast<int>(kMaxStackFrames));

        StackTrace trace;

        if (count <= 0)
            return trace;

        // Like the Windows path, skip this function's own frame in
        // addition to whatever the caller asked to skip.
        std::size_t start = skipFrames + 1;

        for (std::size_t i = start; i < static_cast<std::size_t>(count); ++i)
        {
            trace.push_back(
                reinterpret_cast<std::uintptr_t>(frames[i])
            );
        }

        return trace;
    }

    SymbolizedStackTrace symbolize(const StackTrace& trace)
    {
        SymbolizedStackTrace result;

        auto& resolver = SymbolResolver::instance();

        for (std::uintptr_t address : trace)
        {
            SymbolizedFrame frame{};
            frame.address = address;

            resolver.resolve(
                address,
                frame.function,
                frame.file,
                frame.line
            );

            result.push_back(std::move(frame));
        }

        return result;
    }

    std::string format(const SymbolizedStackTrace& trace)
    {
        std::string result;

        for (const auto& frame : trace)
        {
            if (!frame.function.empty())
            {
                result += frame.function;
            }
            else
            {
                char buffer[64];

                std::snprintf(
                    buffer,
                    sizeof(buffer),
                    "0x%llX",
                    static_cast<unsigned long long>(frame.address)
                );

                result += buffer;
            }

            if (!frame.file.empty())
            {
                result += " - ";
                result += frame.file;
                result += ":";

                result += std::to_string(frame.line);
            }

            result += "\n";
        }

        return result;
    }
}

#else

// No stack-trace implementation for this platform yet (only Windows and
// Linux are done). Allocations are still tracked and reported, just
// without a symbolized (or even raw-address) call stack attached.
namespace PlatformStackTrace
{
    StackTrace capture(std::size_t)
    {
        return {};
    }

    SymbolizedStackTrace symbolize(const StackTrace&)
    {
        return {};
    }

    std::string format(const SymbolizedStackTrace&)
    {
        return {};
    }
}

#endif