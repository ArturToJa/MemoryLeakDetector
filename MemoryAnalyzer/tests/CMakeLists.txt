add_executable(MemoryAnalyzerLeakyTarget
    LeakyTarget.cpp
)

target_compile_features(MemoryAnalyzerLeakyTarget
    PRIVATE
        cxx_std_20
)

target_link_libraries(MemoryAnalyzerLeakyTarget
    PRIVATE
        Threads::Threads
)

# Each scenario below runs MemoryAnalyzerLeakyTarget once, under
# LD_PRELOAD, with its own log file, then checks that file's content.
# CTest's PASS/FAIL_REGULAR_EXPRESSION only inspects a test's own captured
# stdout, so the "check" step pipes the log through `cmake -E cat` rather
# than needing a custom checker binary - the same trick tests/CMakeLists.txt
# would need if it ever checked a file instead of stdout. FIXTURES_SETUP/
# FIXTURES_REQUIRED (not DEPENDS) correctly order "run" before "check" even
# under `ctest -j`/subset selection.
function(add_injection_scenario_test NAME)
    set(logfile "${CMAKE_CURRENT_BINARY_DIR}/${NAME}.log")

    if (WIN32)
        # No LD_PRELOAD on Windows - the launcher starts the target
        # suspended and injects the DLL into it instead.
        add_test(NAME ${NAME}_Run
            COMMAND MemoryAnalyzerLauncher
                $<TARGET_FILE:MemoryAnalyzer>
                $<TARGET_FILE:MemoryAnalyzerLeakyTarget>
        )
        set_tests_properties(${NAME}_Run PROPERTIES
            ENVIRONMENT "MEMANALYZER_LOG_PATH=${logfile}"
        )
    elseif (APPLE)
        # dyld's equivalent of LD_PRELOAD. The dylib carries its own
        # __interpose table, so no flat-namespace flag is needed.
        add_test(NAME ${NAME}_Run COMMAND MemoryAnalyzerLeakyTarget)
        set_tests_properties(${NAME}_Run PROPERTIES
            ENVIRONMENT "DYLD_INSERT_LIBRARIES=$<TARGET_FILE:MemoryAnalyzer>;MEMANALYZER_LOG_PATH=${logfile}"
        )
    else()
        add_test(NAME ${NAME}_Run COMMAND MemoryAnalyzerLeakyTarget)
        set_tests_properties(${NAME}_Run PROPERTIES
            ENVIRONMENT "LD_PRELOAD=$<TARGET_FILE:MemoryAnalyzer>;MEMANALYZER_LOG_PATH=${logfile}"
        )
    endif()

    set_tests_properties(${NAME}_Run PROPERTIES
        PASS_REGULAR_EXPRESSION "LEAKY TARGET END"
        FIXTURES_SETUP ${NAME}
    )

    add_test(NAME ${NAME}_CheckLog COMMAND ${CMAKE_COMMAND} -E cat "${logfile}")
    set_tests_properties(${NAME}_CheckLog PROPERTIES
        FIXTURES_REQUIRED ${NAME}
        ${ARGN}
    )
endfunction()

add_injection_scenario_test(Injection_MallocFreed_NoLeak
    FAIL_REGULAR_EXPRESSION "\\[LEAK\\][^\n]*\\| 211 bytes")

add_injection_scenario_test(Injection_MallocUnfreed_Leaks
    PASS_REGULAR_EXPRESSION "\\[LEAK\\][^\n]*\\| 223 bytes")

add_injection_scenario_test(Injection_NewArrayFreed_NoLeak
    FAIL_REGULAR_EXPRESSION "\\[LEAK\\][^\n]*\\| 227 bytes")

add_injection_scenario_test(Injection_NewArrayUnfreed_Leaks
    PASS_REGULAR_EXPRESSION "\\[LEAK\\][^\n]*\\| 229 bytes")

add_injection_scenario_test(Injection_CallocUnfreed_Leaks
    PASS_REGULAR_EXPRESSION "\\[LEAK\\][^\n]*\\| 233 bytes")

add_injection_scenario_test(Injection_ReallocUnfreed_Leaks
    PASS_REGULAR_EXPRESSION "\\[LEAK\\][^\n]*\\| 239 bytes")

add_injection_scenario_test(Injection_BackgroundThreadFreed_NoLeak
    FAIL_REGULAR_EXPRESSION "\\[LEAK\\][^\n]*\\| 241 bytes")

add_injection_scenario_test(Injection_BackgroundThreadUnfreed_Leaks
    PASS_REGULAR_EXPRESSION "\\[LEAK\\][^\n]*\\| 251 bytes")

if (WIN32)
    # Covers the injector's LoadLibrary hook: a plugin DLL loaded *after*
    # injection must still have its allocations tracked, which only happens
    # if the hook re-patches the module list once the load returns. The
    # target does not link the plugin, so there is no other way for the
    # plugin's import table to get patched.
    add_library(MemoryAnalyzerLeakyPlugin SHARED
        Windows/LeakyPlugin.cpp
    )

    target_compile_features(MemoryAnalyzerLeakyPlugin
        PRIVATE
            cxx_std_20
    )

    add_executable(MemoryAnalyzerLoadLibraryTarget
        Windows/LoadLibraryTarget.cpp
    )

    target_compile_features(MemoryAnalyzerLoadLibraryTarget
        PRIVATE
            cxx_std_20
    )

    add_dependencies(MemoryAnalyzerLoadLibraryTarget
        MemoryAnalyzerLeakyPlugin
    )

    # MODE is which LoadLibrary flavour the target calls (A, W, ExA, ExW).
    function(add_loadlibrary_test MODE)
        set(NAME Injection_LoadLibrary${MODE})
        set(logfile "${CMAKE_CURRENT_BINARY_DIR}/${NAME}.log")

        add_test(NAME ${NAME}_Run
            COMMAND MemoryAnalyzerLauncher
                $<TARGET_FILE:MemoryAnalyzer>
                $<TARGET_FILE:MemoryAnalyzerLoadLibraryTarget>
                $<TARGET_FILE:MemoryAnalyzerLeakyPlugin>
                ${MODE}
        )
        set_tests_properties(${NAME}_Run PROPERTIES
            ENVIRONMENT "MEMANALYZER_LOG_PATH=${logfile}"
            PASS_REGULAR_EXPRESSION "LOADLIB TARGET END"
            FIXTURES_SETUP ${NAME}
        )

        # Both leaks made inside the plugin are reported...
        add_test(NAME ${NAME}_Malloc_Leaks COMMAND ${CMAKE_COMMAND} -E cat "${logfile}")
        set_tests_properties(${NAME}_Malloc_Leaks PROPERTIES
            FIXTURES_REQUIRED ${NAME}
            PASS_REGULAR_EXPRESSION "\\[LEAK\\][^\n]*\\| 3001 bytes"
        )

        add_test(NAME ${NAME}_NewArray_Leaks COMMAND ${CMAKE_COMMAND} -E cat "${logfile}")
        set_tests_properties(${NAME}_NewArray_Leaks PROPERTIES
            FIXTURES_REQUIRED ${NAME}
            PASS_REGULAR_EXPRESSION "\\[LEAK\\][^\n]*\\| 3007 bytes"
        )

        # ...and the block it freed is not.
        add_test(NAME ${NAME}_Freed_NotReported COMMAND ${CMAKE_COMMAND} -E cat "${logfile}")
        set_tests_properties(${NAME}_Freed_NotReported PROPERTIES
            FIXTURES_REQUIRED ${NAME}
            FAIL_REGULAR_EXPRESSION "\\[LEAK\\][^\n]*\\| 3003 bytes"
        )
    endfunction()

    add_loadlibrary_test(A)
    add_loadlibrary_test(W)
    add_loadlibrary_test(ExA)
    add_loadlibrary_test(ExW)
endif()
