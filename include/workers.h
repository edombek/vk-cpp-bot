#ifndef WORKERS_H
#define WORKERS_H
#include "common.h"
#include "events.h"
#include "vk.h"
#include <mutex>
#include <thread>
#define maxEvents 64
class Workers {
private:
    std::thread* thrs;
    uint8_t thrs_count;
    Event** events;
    uint8_t event_read;
    uint8_t event_write;
    uint8_t event_size;
    std::mutex events_lock;
    std::mutex getter_lock;

public:
    Workers(uint8_t c = 1);
    virtual ~Workers();
    void add_event(Event*);
    Event* get_event();
    void work();
};

#endif // WORKERS_H
