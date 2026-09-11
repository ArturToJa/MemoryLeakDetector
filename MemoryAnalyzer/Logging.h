#pragma once

#include <ostream>

// Where the injection library reports leaks to - a log file rather than
// the target process's own stdout/stderr, since an arbitrary target may
// have no usable console (or may be actively using its own stdout for its
// own purposes).
namespace Logging
{
    // Lazily resolves the log path and opens the file on first call. The
    // path comes from MEMANALYZER_LOG_PATH if set, otherwise a per-process
    // file (PID-qualified, to avoid collisions between concurrently
    // injected processes) under the system temp directory.
    std::ostream& stream();
}
