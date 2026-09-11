#pragma once

#include <cstdint>
#include <mutex>
#include <string>

// The underlying dbghelp APIs (SymFromAddr, SymGetLineFromAddr64, ...) are
// documented by Microsoft as not thread-safe - all calls against a given
// process handle must be serialized. Today this class is only ever driven
// from a single thread (initialize/shutdown from Runtime, resolve from
// Tracker::reportLeaks after the tracker thread has been joined), but the
// mutex is here so that stays true even if a future caller resolves
// concurrently (e.g. live/incremental reporting while allocations continue).
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

    std::mutex mutex;
    bool initialized = false;
};