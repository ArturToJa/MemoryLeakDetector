add_executable(TestApp
    TestApp.cpp
)

target_link_libraries(TestApp
    PRIVATE
        MemoryAnalyzerCore
)

target_compile_features(TestApp
    PRIVATE
        cxx_std_20
)

if (MSVC)
    target_link_options(TestApp
        PRIVATE
            "/WHOLEARCHIVE:$<TARGET_FILE:MemoryAnalyzerCore>"
    )
endif()