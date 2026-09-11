add_library(MemoryAnalyzerEngine STATIC
    Tracker.cpp
    Tracker.h

    EventQueue.cpp
    EventQueue.h

    TrackerThread.cpp
    TrackerThread.h

    Events.h

    InterceptorGuard.cpp
    InterceptorGuard.h

    PlatformMemory.cpp
    PlatformMemory.h

    Platform/StackTrace/PlatformStackTrace.cpp
    Platform/StackTrace/PlatformStackTrace.h

    Platform/StackTrace/SymbolResolver.cpp
    Platform/StackTrace/SymbolResolver.h
)

target_include_directories(MemoryAnalyzerEngine
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_features(MemoryAnalyzerEngine
    PUBLIC
        cxx_std_20
)

target_link_libraries(MemoryAnalyzerEngine
    PUBLIC
        Threads::Threads
        ${CMAKE_DL_LIBS}
)

if (WIN32)
    target_link_libraries(MemoryAnalyzerEngine
        PRIVATE
            dbghelp
    )
endif()
