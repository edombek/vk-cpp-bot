#ifndef EVENTS_H_INCLUDED
#define EVENTS_H_INCLUDED
#include "common.h"
#include "doc.h"
#include "users.h"
#include "vk.h"

class Event {
public:
    Net& net;
    Vk& vk;
    Event(Net& n, Vk& v, json lpEv = NULL);
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
};

#endif // EVENTS_H_INCLUDED
