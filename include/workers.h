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
    std::thread** thrs;
    uint8_t thrs_count;
    json* events;
    uint8_t event_read;
    uint8_t event_write;
    uint8_t event_size;
    std::recursive_mutex events_lock;
    std::recursive_mutex getter_lock;

public:
    Workers(uint8_t c = 1);
    ~Workers();
    void add_event(json);
    json get_event();
    void work();
};

#endif // WORKERS_H
