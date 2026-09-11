#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace PlatformStackTrace
{
    constexpr std::size_t kMaxStackFrames = 32;

    using StackTrace = std::vector<std::uintptr_t>;

    struct SymbolizedFrame
    {
        std::uintptr_t address;

        std::string function;
        std::string file;

        std::uint32_t line = 0;
    };

    using SymbolizedStackTrace = std::vector<SymbolizedFrame>;

    StackTrace capture(std::size_t skipFrames = 0);

    SymbolizedStackTrace symbolize(const StackTrace& trace);

    std::string format(const SymbolizedStackTrace& trace);
}