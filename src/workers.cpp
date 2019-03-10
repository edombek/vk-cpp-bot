#include "workers.h"

using namespace std;

Workers::Workers(uint8_t c)
{
    this->thrs = new thread[c];
    this->thrs_count = c;
    this->events = new Event*[maxEvents];
    this->event_read = 0;
    this->event_write = 0;
    this->event_size = 0;
    for (int i = 0; i < c; i++) {
        this->thrs[i] = thread(&Workers::work, this);
    }
}

Workers::~Workers()
{
    for (int i = 0; i < this->thrs_count; i++)
        this->thrs[i].join();
    delete thrs;
    delete events;
}

void Workers::add_event(Event* event)
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
    this->getter_lock.try_lock();
    this->getter_lock.unlock();
}

Event* Workers::get_event()
{
    Event* event = NULL;
    this->getter_lock.lock();
    this->events_lock.lock();
    if (this->event_size) {

        event = this->events[this->event_read];
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
    Vk vk(&net);
    while (true) {
        Event* event = this->get_event();
        if (event == NULL)
            continue;
        for (auto doc : event->docs) {
            net.send(doc->url);
            //Doc d(doc->type, to_string(event->peer_id), "doc." + doc->ext, net.buffer, &net, &vk);
        }
        if (event->msg.size() && event->from_id > 0) {
            event->user = users::getUser(event->from_id, &vk);
            Event* outEvent = event->getOut();
            event->setNet(&net, &vk);
            outEvent->setNet(&net, &vk);
            outEvent->msg += outEvent->user.name + ", ";
            if (cmd::start(str::low(*event->words.begin()), event, outEvent))
                outEvent->send();
            delete outEvent;
            bd.save();
        }
        delete event;
    }
}
