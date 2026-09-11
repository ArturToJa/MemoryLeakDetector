#pragma once

// Reentrancy guard for the *producer* side: capturing a stack trace and
// pushing an event onto the queue (NewDelete.cpp, EventQueue) can itself
// allocate. Without this guard that allocation would recurse back into
// trackAllocation/trackDeallocation on the same thread. See TrackingGuard
// in Tracker.h for the analogous guard on the *consumer* side (the tracker
// thread touching the allocations map / iostream) - they guard independent
// call paths that run on different threads, which is why there are two.
class InterceptorGuard
{
public:
    InterceptorGuard();
    ~InterceptorGuard();

    static bool isDisabled();

private:
    static thread_local int depth;
};