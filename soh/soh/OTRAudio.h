#pragma once
#include <thread>
#include <condition_variable>

static struct {
    std::thread thread;
    std::condition_variable cv_to_thread;
    std::mutex mutex;
    std::atomic_bool running;
    std::atomic_bool processing;
#ifdef DIPTYCH_GAME_MODULE
    std::condition_variable cv_from_thread;
    std::atomic_bool suspended;
    bool parked;
#endif
} audio;
