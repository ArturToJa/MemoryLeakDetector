#pragma once

#include "Events.h"

#include <condition_variable>
#include <mutex>
#include <optional>
#include <queue>

class EventQueue
{
public:
    void push(const AllocationEvent& event);

    std::optional<AllocationEvent> waitAndPop();

    void stop();

private:
    std::queue<AllocationEvent> queue;
    std::mutex mutex;
    std::condition_variable condition;
    bool stopped = false;
};