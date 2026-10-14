// An ordinary executable that loads a plugin DLL at runtime, after the
// injector is already in place, and calls into it. It deliberately does NOT
// link the plugin: the only way the plugin's allocations can be tracked is
// if the injector's LoadLibrary hook patches it once it is loaded.
//
//   LoadLibraryTarget <plugin.dll> <A|W|ExA|ExW>
//
// The second argument picks which LoadLibrary flavour to use, since each is
// hooked separately.

#include <windows.h>

#include <cstdio>
#include <cstring>
#include <string>

namespace
{
    std::wstring widen(const char* s)
    {
        int n = MultiByteToWideChar(CP_ACP, 0, s, -1, nullptr, 0);
        std::wstring w(static_cast<std::size_t>(n), L'\0');
        MultiByteToWideChar(CP_ACP, 0, s, -1, w.data(), n);
        return w;
    }
}

int main(int argc, char** argv)
{
    if (argc < 3)
    {
        std::fprintf(stderr, "usage: LoadLibraryTarget <plugin.dll> <A|W|ExA|ExW>\n");
        return 2;
    }

    std::printf("LOADLIB TARGET START\n");

    const char* path = argv[1];
    const char* mode = argv[2];
    HMODULE plugin = nullptr;

    if (std::strcmp(mode, "A") == 0)
        plugin = LoadLibraryA(path);
    else if (std::strcmp(mode, "W") == 0)
        plugin = LoadLibraryW(widen(path).c_str());
    else if (std::strcmp(mode, "ExA") == 0)
        plugin = LoadLibraryExA(path, nullptr, 0);
    else if (std::strcmp(mode, "ExW") == 0)
        plugin = LoadLibraryExW(widen(path).c_str(), nullptr, 0);

    if (!plugin)
    {
        std::fprintf(stderr, "could not load plugin (%lu)\n", GetLastError());
        return 3;
    }

    auto leak = reinterpret_cast<void (*)()>(GetProcAddress(plugin, "leakInPlugin"));

    if (!leak)
    {
        std::fprintf(stderr, "leakInPlugin not found\n");
        return 4;
    }

    leak();

    std::printf("LOADLIB TARGET END\n");

    return 0;
}
