# Run this target from the IDE to see the Windows injection library work end
# to end: building it also builds MemoryAnalyzer.dll and
# MemoryAnalyzerLauncher.exe, and running it re-launches itself through the
# launcher with the DLL injected, then prints the leak report. See
# TestDLLApp.cpp for how.
add_executable(TestDLLApp
    TestDLLApp.cpp
)

target_compile_features(TestDLLApp
    PRIVATE
        cxx_std_20
)

# Not linked against either (the app must stay an ordinary, unaware
# program) - only ordered after them, and told where they ended up.
add_dependencies(TestDLLApp
    MemoryAnalyzer
    MemoryAnalyzerLauncher
)

target_compile_definitions(TestDLLApp
    PRIVATE
        "LAUNCHER_PATH=\"$<TARGET_FILE:MemoryAnalyzerLauncher>\""
        "ANALYZER_DLL_PATH=\"$<TARGET_FILE:MemoryAnalyzer>\""
)
