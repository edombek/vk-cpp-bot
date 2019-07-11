#include "worker.h"

using namespace std;

Worker::Worker()
{
    this->thr = new thread(&Worker::work, this);
}

Worker::~Worker()
{
    this->thr->join();
    delete this->thr;
}

void Worker::add_event(json event)
{
    std::unique_lock<std::mutex> lock(this->m);
    this->events.push(event);
    this->cond_var.notify_one();
}

json Worker::get_event()
{
    json event;
    std::unique_lock<std::mutex> lock(this->m);
    while (!this->events.size())
        this->cond_var.wait(lock);
    event = this->events.front();
    this->events.pop();
    return event;
}

#include "cmd.h"
#include "str.h"
#include <iostream>
void Worker::work()
{
    Net net;
    Vk vk(net);
    while (true)
    {
        json event = this->get_event();
        if (event.is_null())
            continue;
        Event eventOut(net, vk, event);
        if (eventOut.msg.size() && eventOut.from_id > 0)
        {
            eventOut.user = users::getUser(eventOut.from_id, vk);
            Event outEvent = eventOut.getOut();
            outEvent.msg += outEvent.user.name + ", ";
            if (cmd::start(str::low(*eventOut.words.begin()), eventOut, outEvent, this))
                outEvent.send();
            bd.save();
        }
    }
}
