#include "pch.h"
#include "InterceptorGuard.h"

thread_local int InterceptorGuard::depth = 0;

InterceptorGuard::InterceptorGuard()
{
    ++depth;
}

InterceptorGuard::~InterceptorGuard()
{
    --depth;
}

bool InterceptorGuard::isDisabled()
{
    return depth > 0;
}