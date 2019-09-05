#include "worker.h"

using namespace std;

void Worker::add_event(json event)
{
    this->pool.enqueue([this](json event)
    {
        this->work(event);
    }, event);
}

#include "cmd.h"
#include "str.h"
#include <iostream>
void Worker::work(json event)
{
    Net net;
    Vk vk(net);
    if (event.is_null())
        return;
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
