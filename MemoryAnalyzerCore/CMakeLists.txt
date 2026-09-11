add_library(MemoryAnalyzerCore STATIC
    Runtime.cpp
    Runtime.h

    NewDelete.cpp
)

target_include_directories(MemoryAnalyzerCore
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_features(MemoryAnalyzerCore
    PUBLIC
        cxx_std_20
)

target_link_libraries(MemoryAnalyzerCore
    PUBLIC
        MemoryAnalyzerEngine
)
