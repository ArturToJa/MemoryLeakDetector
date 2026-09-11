# Only the Linux LD_PRELOAD injection library is implemented so far
# (Phase 2 of the injection-DLL plan). Windows (DLL injection + IAT
# patching) and macOS (DYLD_INTERPOSE) are later phases; this guard keeps
# their CI jobs configuring cleanly against an empty subdirectory until
# then, rather than needing a top-level CMakeLists.txt condition.
if (UNIX AND NOT APPLE)
    add_library(MemoryAnalyzer MODULE
        Session.cpp
        Session.h

        Logging.cpp
        Logging.h

        NewDeleteHooks.cpp

        Linux/RealAllocator.cpp
        Linux/RealAllocator.h

        Linux/MallocHooks.cpp
    )

    target_include_directories(MemoryAnalyzer
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}
    )

    target_compile_features(MemoryAnalyzer
        PRIVATE
            cxx_std_20
    )

    target_link_libraries(MemoryAnalyzer
        PRIVATE
            MemoryAnalyzerEngine
            ${CMAKE_DL_LIBS}
    )

    add_subdirectory(tests)
endif()
