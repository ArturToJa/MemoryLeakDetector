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

# End-to-end: run the app exactly as F5 does (it re-launches itself through
# the injector and prints the leak log), then check the report. Each test
# gets its own log file so they can run in parallel under `ctest -j`.
function(add_testdllapp_test NAME)
    add_test(NAME ${NAME} COMMAND TestDLLApp)
    set_tests_properties(${NAME} PROPERTIES
        ENVIRONMENT "MEMANALYZER_LOG_PATH=${CMAKE_CURRENT_BINARY_DIR}/${NAME}.log"
        ${ARGN}
    )
endfunction()

add_testdllapp_test(TestDLLApp_Malloc_Leaks
    PASS_REGULAR_EXPRESSION "\\[LEAK\\][^\n]*\\| 2001 bytes")

add_testdllapp_test(TestDLLApp_NewArray_Leaks
    PASS_REGULAR_EXPRESSION "\\[LEAK\\][^\n]*\\| 2003 bytes")

add_testdllapp_test(TestDLLApp_Realloc_ReportsGrownSize
    PASS_REGULAR_EXPRESSION "\\[LEAK\\][^\n]*\\| 2005 bytes")

add_testdllapp_test(TestDLLApp_VectorBuffer_Leaks
    PASS_REGULAR_EXPRESSION "\\[LEAK\\][^\n]*\\| 2000 bytes")

add_testdllapp_test(TestDLLApp_FreedMalloc_NotReported
    FAIL_REGULAR_EXPRESSION "\\[LEAK\\][^\n]*\\| 1001 bytes")

add_testdllapp_test(TestDLLApp_FreedNewArray_NotReported
    FAIL_REGULAR_EXPRESSION "\\[LEAK\\][^\n]*\\| 1003 bytes")

# The report is symbolized down to the leaking line in the app's own source.
add_testdllapp_test(TestDLLApp_Report_HasSourceLocation
    PASS_REGULAR_EXPRESSION "TestDLLApp\\.cpp:[0-9]+")

# Invoking the launcher directly (what the app does internally) runs the
# target with the DLL injected, and a zero exit code passes the test.
add_test(NAME TestDLLApp_Launcher_RunsTargetInjected
    COMMAND MemoryAnalyzerLauncher
        $<TARGET_FILE:MemoryAnalyzer>
        $<TARGET_FILE:TestDLLApp>
)
set_tests_properties(TestDLLApp_Launcher_RunsTargetInjected PROPERTIES
    ENVIRONMENT "MEMANALYZER_LOG_PATH=${CMAKE_CURRENT_BINARY_DIR}/TestDLLApp_Launcher_RunsTargetInjected.log"
    PASS_REGULAR_EXPRESSION "running under the injector"
)
