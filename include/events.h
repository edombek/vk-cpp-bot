#ifndef EVENTS_H_INCLUDED
#define EVENTS_H_INCLUDED
#include "common.h"
#include "vk.h"

class Event {
public:
    Net* net;
    Vk* vk;
    Event(json lpEv = NULL);
    ~Event();
    void setNet(Net* n, Vk* v);
    std::string type;
    std::string msg;
    args_t words;
    int from_id;
    int peer_id;
    int id;
    int random_id;
    int timestamp;
    bool is_chat;
    bool copied;
    Event* copy();
    Event* getOut();
    std::vector<Doc*> docs;
    std::vector<Event*> fwds;
    json send();
};

#endif // EVENTS_H_INCLUDED
