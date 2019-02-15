#ifndef WORKERS_H
#define WORKERS_H
#include "common.h"
#include "vk.h"
#include <mutex>
#include <thread>
class Workers {
private:
    std::vector<std::thread> thrs;
    std::vector<Event*> events;
    std::mutex events_lock;
    std::mutex getter_lock;

public:
    Workers(int c = 1);
    ~Workers();
    void add_event(Event*);
    Event* get_event();
    void work();
};

#endif // WORKERS_H
