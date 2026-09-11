#pragma once

class InterceptorGuard
{
public:
    InterceptorGuard();
    ~InterceptorGuard();

    static bool isDisabled();

private:
    static thread_local int depth;
};