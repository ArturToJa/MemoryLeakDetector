add_executable(MemoryAnalyzerCoreTests
    TrackerTests.cpp
)

target_link_libraries(MemoryAnalyzerCoreTests
    PRIVATE
        MemoryAnalyzerCore
)

target_compile_features(MemoryAnalyzerCoreTests
    PRIVATE
        cxx_std_20
)

add_test(
    NAME MemoryAnalyzerCoreTests
    COMMAND MemoryAnalyzerCoreTests
)

# LifetimeScenarios exercises the real global operator new/delete
# interception end-to-end (not Tracker directly), covering static/global
# object lifetimes and cross-thread allocation that TrackerTests can't
# reach. Each scenario allocates a distinct, unusual byte count so a single
# run's output can be checked per-scenario via regex - see
# LifetimeScenarios.cpp for what each size/scenario means.
add_executable(MemoryAnalyzerLifetimeTests
    LifetimeScenarios.cpp
)

target_link_libraries(MemoryAnalyzerLifetimeTests
    PRIVATE
        MemoryAnalyzerCore
)

target_compile_features(MemoryAnalyzerLifetimeTests
    PRIVATE
        cxx_std_20
)

add_test(NAME Lifetime_LocalFreed_NoLeak COMMAND MemoryAnalyzerLifetimeTests)
set_tests_properties(Lifetime_LocalFreed_NoLeak PROPERTIES
    FAIL_REGULAR_EXPRESSION "\\[LEAK\\][^\n]*\\| 101 bytes"
)

add_test(NAME Lifetime_LocalUnfreed_Leaks COMMAND MemoryAnalyzerLifetimeTests)
set_tests_properties(Lifetime_LocalUnfreed_Leaks PROPERTIES
    PASS_REGULAR_EXPRESSION "\\[LEAK\\][^\n]*\\| 103 bytes"
)

add_test(NAME Lifetime_GlobalObject_DestructedBeforeReport_NoLeak COMMAND MemoryAnalyzerLifetimeTests)
set_tests_properties(Lifetime_GlobalObject_DestructedBeforeReport_NoLeak PROPERTIES
    FAIL_REGULAR_EXPRESSION "\\[LEAK\\][^\n]*\\| 149 bytes"
)

add_test(NAME Lifetime_GlobalRawPointer_NeverFreed_Leaks COMMAND MemoryAnalyzerLifetimeTests)
set_tests_properties(Lifetime_GlobalRawPointer_NeverFreed_Leaks PROPERTIES
    PASS_REGULAR_EXPRESSION "\\[LEAK\\][^\n]*\\| 151 bytes"
)

add_test(NAME Lifetime_FunctionLocalStatic_NoLeak COMMAND MemoryAnalyzerLifetimeTests)
set_tests_properties(Lifetime_FunctionLocalStatic_NoLeak PROPERTIES
    FAIL_REGULAR_EXPRESSION "\\[LEAK\\][^\n]*\\| 157 bytes"
)

add_test(NAME Lifetime_BackgroundThread_FreedBeforeJoin_NoLeak COMMAND MemoryAnalyzerLifetimeTests)
set_tests_properties(Lifetime_BackgroundThread_FreedBeforeJoin_NoLeak PROPERTIES
    FAIL_REGULAR_EXPRESSION "\\[LEAK\\][^\n]*\\| 163 bytes"
)

add_test(NAME Lifetime_BackgroundThread_Unfreed_Leaks COMMAND MemoryAnalyzerLifetimeTests)
set_tests_properties(Lifetime_BackgroundThread_Unfreed_Leaks PROPERTIES
    PASS_REGULAR_EXPRESSION "\\[LEAK\\][^\n]*\\| 167 bytes"
)
