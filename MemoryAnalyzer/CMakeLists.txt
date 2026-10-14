# Linux (LD_PRELOAD, Phase 2) and Windows (launcher + DLL injection + IAT
# patching, Phase 3) are implemented. macOS (DYLD_INTERPOSE) is a later
# phase; this guard keeps its CI job configuring cleanly against an empty
# subdirectory until then, rather than needing a top-level CMakeLists.txt
# condition.
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

if (WIN32)
    add_library(MemoryAnalyzer MODULE
        Session.cpp
        Session.h

        Logging.cpp
        Logging.h

        Windows/AllocHooks.cpp
        Windows/AllocHooks.h
        Windows/DllMain.cpp
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
    )

    # Starts a target suspended, injects the DLL, resumes it.
    add_executable(MemoryAnalyzerLauncher
        Windows/Launcher.cpp
    )

    target_compile_features(MemoryAnalyzerLauncher
        PRIVATE
            cxx_std_20
    )

    # Always a wide-char entry point, regardless of toolchain defaults.
    if (MSVC)
        target_link_options(MemoryAnalyzerLauncher PRIVATE /ENTRY:wmainCRTStartup)
    endif()

    add_subdirectory(tests)
endif()
