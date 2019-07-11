#pragma once
#include "common.h"
#include "events.h"
#include "vk.h"
#include <condition_variable>
#include <mutex>
#include <thread>
#include <queue>
class Worker
{
private:
    std::thread* thr;
    std::queue<json> events;
    std::mutex m;
    std::condition_variable cond_var;
public:
    Worker();
    ~Worker();
    void add_event(json);
    json get_event();
    void work();
};
