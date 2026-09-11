#include "pch.h"
#include "TrackerThread.h"
#include <iostream>

TrackerThread::TrackerThread(
    EventQueue& queue,
    Tracker& tracker)
    : queue(queue),
    tracker(tracker)
{
}

void TrackerThread::start()
{
    worker = std::thread(&TrackerThread::run, this);
}

void TrackerThread::stop()
{
    queue.stop();

    if (worker.joinable())
    {
        worker.join();
    }
}

void TrackerThread::run()
{
    while (true)
    {
        auto event = queue.waitAndPop();

        if (!event)
        {
            break;
        }

        switch (event->type)
        {
        case EventType::Allocate:
            tracker.onAllocate(
                event->address,
                event->size,
                event->stackTrace
            );

            break;

        case EventType::Deallocate:
            tracker.onDeallocate(
                event->address
            );

            break;
        }
    }
}