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
