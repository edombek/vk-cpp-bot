#pragma once
#include "common.h"
#include "doc.h"
#include "users.h"
#include "vk.h"

class Event
{
public:
    Net& net;
    Vk& vk;
    Event(Net& n, Vk& v, json lpEv = NULL);
    Event(const Event& e);
    Event& operator=(const Event& e)
    {
        Event event(e);
        return event;
    };
    bool operator==(const Event& e)
    {
        return this->id==e.id;
    };
    std::string type;
    std::string msg;
    args_t words;
    int32_t from_id;
    uint32_t peer_id;
    uint32_t id;
    uint32_t random_id;
    time_t timestamp;
    bool is_chat;
    users::user user;
    Event getOut();
    std::vector<Doc> docs;
    std::vector<Event> fwds;
    json send();
    std::string pysend();
};

