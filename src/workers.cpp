#include "workers.h"

using namespace std;

Workers::Workers(uint8_t c)
{
    this->thrs = new thread*[c];
    this->thrs_count = c;
    this->events = new json[maxEvents];
    this->event_read = 0;
    this->event_write = 0;
    this->event_size = 0;
    for (int i = 0; i < c; i++) {
        this->thrs[i] = new thread(&Workers::work, this);
    }
    stopped = false;
}

Workers::~Workers()
{
    for (int i = 0; i < this->thrs_count; i++) {
        this->thrs[i]->join();
        delete this->thrs[i];
    }
    delete[] thrs;
    delete[] this->events;
}

void Workers::stop()
{
    for (int i = 0; i < maxEvents; i++)
        this->add_event("stop");
    this->stopped = true;
}

void Workers::add_event(json event)
{
    this->events_lock.lock();
    if (this->event_size == maxEvents) {
        this->events_lock.unlock();
        return;
    }
    this->event_size++;
    this->events[this->event_write] = event;
    this->event_write = (this->event_write + 1) % maxEvents;
    this->events_lock.unlock();
    if (!this->getter_lock.try_lock())
        this->getter_lock.unlock();
}

json Workers::get_event()
{
    json event;
    this->getter_lock.lock();
    this->events_lock.lock();
    if (this->event_size) {
        event = this->events[this->event_read];
        this->events[this->event_read] = json::value_t::null;
        this->event_read = (this->event_read + 1) % maxEvents;
        this->event_size--;
    }
    if (this->event_size)
        this->getter_lock.unlock();
    this->events_lock.unlock();
    return event;
}

#include "cmd.h"
#include "str.h"
#include <iostream>
void Workers::work()
{
    Net net;
    Vk vk(net);
    while (true) {
        json event = this->get_event();
        if (event.is_null())
            continue;
        if (event.is_string() && event == "stop")
            return;
        Event eventOut(net, vk, event);
        if (eventOut.msg.size() && eventOut.from_id > 0) {
            eventOut.user = users::getUser(eventOut.from_id, vk);
            Event outEvent = eventOut.getOut();
            outEvent.msg += outEvent.user.name + ", ";
            if (cmd::start(str::low(*eventOut.words.begin()), eventOut, outEvent, this))
                outEvent.send();
            bd.save();
        }
    }
}
