#include "Logging.h"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>

#include <unistd.h>

namespace
{
    std::string resolveLogPath()
    {
        const char* envPath = std::getenv("MEMANALYZER_LOG_PATH");

        if (envPath && *envPath)
            return envPath;

        const char* tmpDir = std::getenv("TMPDIR");

        std::ostringstream path;
        path << (tmpDir && *tmpDir ? tmpDir : "/tmp")
             << "/memoryanalyzer-" << getpid() << ".log";

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
