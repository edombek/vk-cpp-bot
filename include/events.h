#ifndef EVENTS_H_INCLUDED
#define EVENTS_H_INCLUDED
#include "common.h"
#include "vk.h"

class Event {
public:
    Event(json lpEv = NULL);
    ~Event();
    std::string type;
    std::string msg;
    int from_id;
    int peer_id;
    int post_id;
    int random_id;
    int timestamp;
    bool is_chat;
    std::vector<Doc*> docs;
    std::vector<Event*> fwds;
    json send(Vk* vk);
};

#endif // EVENTS_H_INCLUDED
