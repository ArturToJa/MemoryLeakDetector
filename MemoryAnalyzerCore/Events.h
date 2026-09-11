#pragma once
#include "Platform/StackTrace/PlatformStackTrace.h"

#include <cstddef>

enum class EventType
{
    Allocate,
    Deallocate
};

struct AllocationEvent
{
    EventType type;
    void* address;
    std::size_t size;

    PlatformStackTrace::StackTrace stackTrace;
};