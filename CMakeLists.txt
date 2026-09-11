cmake_minimum_required(VERSION 3.21)

project(MemoryAnalyzer
    VERSION 0.1.0
    LANGUAGES CXX
)

enable_testing()

add_subdirectory(MemoryAnalyzerCore)
add_subdirectory(TestApp)
add_subdirectory(tests)
