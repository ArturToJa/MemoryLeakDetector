cmake_minimum_required(VERSION 3.20)

project(MemoryAnalyzer
    VERSION 0.1.0
    LANGUAGES CXX
)

add_subdirectory(MemoryAnalyzerCore)
add_subdirectory(TestApp)