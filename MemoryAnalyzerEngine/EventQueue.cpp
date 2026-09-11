#include "pch.h"
#include "EventQueue.h"
#include "InterceptorGuard.h"
#include <iostream>

void EventQueue::push(const AllocationEvent& event)
{
    InterceptorGuard guard;

    {
        std::lock_guard<std::mutex> lock(mutex);

        if (stopped)
            return;

        queue.push(event);
    }

    condition.notify_one();
}

std::optional<AllocationEvent> EventQueue::waitAndPop()
{
    InterceptorGuard guard;

    std::unique_lock<std::mutex> lock(mutex);

    condition.wait(lock, [this]
        {
            return !queue.empty() || stopped;
        });

    if (queue.empty() && stopped)
    {
        return std::nullopt;
    }

    AllocationEvent event = queue.front();
    queue.pop();

    return event;
}

void EventQueue::stop()
{
    {
        std::lock_guard<std::mutex> lock(mutex);
        stopped = true;
    }

    condition.notify_all();
}