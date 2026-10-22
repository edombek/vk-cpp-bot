#pragma once
#include "common.h"
#include "doc.h"
#include "users.h"
#include "vk.h"

class Event
{
private:
    void copyBody(const Event& e); // общие поля, кроме ссылок net/vk (они не переприсваиваются)
public:
    Net& net;
    Vk& vk;
    Event(Net& n, Vk& v, json lpEv = NULL);
    Event(const Event& e);
    Event& operator=(const Event& e)
    {
        if (this != &e) // ссылки net/vk не переприсваиваются — остальное копируем
            this->copyBody(e);
        return *this;
    };
    bool operator==(const Event& e) const
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
    std::string event_id; // для message_event (нажатие callback-кнопки)
    std::string keyboard; // JSON Keyboard — прикрепляется к ответу (messages.send)
    users::user user = {};
    Event getOut() const; // новый пустой ответ на основе этого события
    std::vector<Doc> docs;
    std::vector<Event> fwds;
    json send();
    std::string pysend();
};
