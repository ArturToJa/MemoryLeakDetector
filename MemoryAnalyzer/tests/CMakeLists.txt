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

    add_test(NAME ${NAME}_Run COMMAND MemoryAnalyzerLeakyTarget)
    set_tests_properties(${NAME}_Run PROPERTIES
        ENVIRONMENT "LD_PRELOAD=$<TARGET_FILE:MemoryAnalyzer>;MEMANALYZER_LOG_PATH=${logfile}"
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
