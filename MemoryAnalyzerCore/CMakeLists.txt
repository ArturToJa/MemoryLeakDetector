add_library(MemoryAnalyzerCore STATIC
    Tracker.cpp
    Tracker.h

    EventQueue.cpp
    EventQueue.h

    TrackerThread.cpp
    TrackerThread.h

    Runtime.cpp
    Runtime.h

    Events.h

    InterceptorGuard.cpp
    InterceptorGuard.h

    PlatformMemory.cpp
    PlatformMemory.h

    Platform/StackTrace/PlatformStackTrace.cpp
    Platform/StackTrace/PlatformStackTrace.h

    Platform/StackTrace/SymbolResolver.cpp
    Platform/StackTrace/SymbolResolver.h

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

if (WIN32)
    target_link_libraries(MemoryAnalyzerCore
        PRIVATE
            dbghelp
    )
endif()