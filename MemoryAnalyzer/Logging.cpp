#include "Logging.h"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>

#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

namespace
{
    std::string resolveLogPath()
    {
        const char* envPath = std::getenv("MEMANALYZER_LOG_PATH");

        if (envPath && *envPath)
            return envPath;

#ifdef _WIN32
        const char* tmpDir = std::getenv("TEMP");
        const char* fallbackDir = ".";
        const int pid = _getpid();
#else
        const char* tmpDir = std::getenv("TMPDIR");
        const char* fallbackDir = "/tmp";
        const int pid = getpid();
#endif

        std::ostringstream path;
        path << (tmpDir && *tmpDir ? tmpDir : fallbackDir)
             << "/memoryanalyzer-" << pid << ".log";

        return path.str();
    }
}

namespace Logging
{
    std::ostream& stream()
    {
        static std::string path = resolveLogPath();
        static std::ofstream file(path, std::ios::out | std::ios::trunc);

        static bool noticePrinted = []
        {
            std::cerr << "[MemoryAnalyzer] logging leaks to " << path << "\n";
            return true;
        }();
        (void)noticePrinted;

        return file;
    }
}
