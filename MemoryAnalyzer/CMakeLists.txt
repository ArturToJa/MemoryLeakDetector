# Linux (LD_PRELOAD, Phase 2), Windows (launcher + DLL injection + IAT
# patching, Phase 3) and macOS (DYLD_INSERT_LIBRARIES + DYLD_INTERPOSE,
# Phase 4) are each implemented in their own block below.
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

if (APPLE)
    # SHARED, not MODULE: DYLD_INSERT_LIBRARIES needs a real dylib, and
    # CMake's MODULE on macOS produces a loadable bundle, which dyld refuses
    # to insert.
    add_library(MemoryAnalyzer SHARED
        Session.cpp
        Session.h

        Logging.cpp
        Logging.h

        macOS/InterposeHooks.cpp
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
