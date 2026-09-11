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

#else

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