#include "pch.h"
#include "EventQueue.h"
#include "Tracker.h"
#include "TrackerThread.h"

extern "C" __declspec(dllexport)
void MyMemTest()
{
    EventQueue queue;

    Tracker tracker;

    TrackerThread trackerThread(queue, tracker);

    trackerThread.start();

    int* a = new int[100];
    int* b = new int;

    queue.push({
        EventType::Allocate,
        a,
        sizeof(int) * 100
        });

    queue.push({
        EventType::Allocate,
        b,
        sizeof(int)
        });

    queue.push({
        EventType::Deallocate,
        b,
        0
        });

    delete b;

    trackerThread.stop();

    tracker.reportLeaks();
}