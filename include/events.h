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
        if (this != &e) // ссылки net/vk не переприсваиваются — остальное копируем
        {
            this->type = e.type;
            this->msg = e.msg;
            this->words = e.words;
            this->from_id = e.from_id;
            this->peer_id = e.peer_id;
            this->id = e.id;
            this->random_id = e.random_id;
            this->timestamp = e.timestamp;
            this->is_chat = e.is_chat;
            this->user = e.user;
            this->docs = e.docs;
            this->fwds = e.fwds;
        }
        return *this;
    };
    bool operator==(const Event& e)
    {
        return this->id==e.id;
    };
    std::string type;
    std::string msg;
    args_t words;
    int32_t from_id = 0;
    uint32_t peer_id = 0;
    uint32_t id = 0;
    uint32_t random_id = 0;
    time_t timestamp = 0;
    bool is_chat = false;
    users::user user = {};
    Event getOut();
    std::vector<Doc> docs;
    std::vector<Event> fwds;
    json send();
    std::string pysend();
};

