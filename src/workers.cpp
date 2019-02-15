#include "workers.h"

using namespace std;

Workers::Workers(int c)
{
    this->thrs.resize(c);
    for (int i = 0; i < c; i++) {
        this->thrs[i] = thread(&Workers::work, this);
    }
}

Workers::~Workers()
{
    for (int i = 0; i < this->thrs.size(); i++)
        this->thrs[i].join();
}

void Workers::add_event(Event* event)
{
    this->events_lock.lock();
    this->events.push_back(event);
    this->events_lock.unlock();
    this->getter_lock.try_lock();
    this->getter_lock.unlock();
}

Event* Workers::get_event()
{
    Event* event = NULL;
    this->getter_lock.lock();
    this->events_lock.lock();
    if (this->events.size()) {
        event = this->events[0];
        this->events.erase(this->events.begin());
    }
    if (this->events.size())
        this->getter_lock.unlock();
    this->events_lock.unlock();
    return event;
}

#include <iostream>
void Workers::work()
{
    Net net;
    Vk vk(&net);
    while (true) {
        Event* event = this->get_event();
        if (event == NULL)
            continue;
        cout << event->msg << endl;
        delete event;
    }
}
