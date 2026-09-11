#pragma once

#include <cstdint>
#include <string>

class SymbolResolver
{
public:
    static SymbolResolver& instance();

    bool initialize();
    void shutdown();

    bool resolve(
        std::uintptr_t address,
        std::string& function,
        std::string& file,
        std::uint32_t& line);

private:
    SymbolResolver() = default;

    bool initialized = false;
};