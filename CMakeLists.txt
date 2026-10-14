cmake_minimum_required(VERSION 3.21)

project(MemoryAnalyzer
    VERSION 0.1.0
    LANGUAGES CXX
)

enable_testing()

find_package(Threads REQUIRED)

add_subdirectory(MemoryAnalyzerEngine)
add_subdirectory(MemoryAnalyzerCore)
add_subdirectory(MemoryAnalyzer)
add_subdirectory(TestApp)
add_subdirectory(tests)

# Windows-only demo that builds and runs the injector against a leaky app.
if (WIN32)
    add_subdirectory(TestDLLApp)
endif()
