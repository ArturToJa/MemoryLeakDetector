#pragma once

#include "EventQueue.h"
#include "Tracker.h"

#include <thread>

class TrackerThread
{
public:
    TrackerThread(EventQueue& queue, Tracker& tracker);

    void start();
    void stop();

private:
    void run();

    EventQueue& queue;
    Tracker& tracker;

    std::thread worker;
};