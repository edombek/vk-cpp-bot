#include "events.h"
#include "common.h"
#include "str.h"
Event::Event(Net &n, Vk &v, json lpEv)
    : net(n)
    , vk(v)
{
    if (lpEv == NULL)
        return;
    this->type = lpEv["type"];
    lpEv = lpEv["object"];
    if (this->type == "message_event") // нажатие кнопки: payload вместо text
    {
        if (lpEv["user_id"].is_number())
            this->from_id = lpEv["user_id"];
        if (lpEv["peer_id"].is_number())
            this->peer_id = lpEv["peer_id"];
        if (lpEv["event_id"].is_string())
            this->event_id = lpEv["event_id"];
        if (lpEv["payload"].is_string())
            this->msg = lpEv["payload"];
        else if (lpEv["text"].is_string()) // inline text-кнопки приходят как text
            this->msg = lpEv["text"];
        this->words = str::words(this->msg);
        this->is_chat = uint32_t(this->from_id) != this->peer_id;
        return;
    }
    if (lpEv["random_id"].is_number())
        this->random_id = lpEv["random_id"];
    else
        this->random_id = 0;
    this->msg = lpEv["text"];
    this->words = str::words(this->msg);
    this->from_id = lpEv["from_id"];
    if (this->from_id < 0)
        return;
    this->timestamp = lpEv["date"];
    if (this->type.find("message_") != this->type.npos)
    {
        if(lpEv["peer_id"].is_number())
            this->peer_id = lpEv["peer_id"];
        if (!lpEv["reply_message"].is_null())
            this->fwds.emplace_back(Event(this->net, this->vk, { { "type", this->type }, { "object", lpEv["reply_message"] } }));
        if (!lpEv["fwd_messages"].is_null())
            for (auto fwd : lpEv["fwd_messages"])
                this->fwds.emplace_back(Event(this->net, this->vk, { { "type", this->type }, { "object", fwd } }));
    }
    if (this->type.find("wall_post_") != this->type.npos)
    {
        this->peer_id = lpEv["owner_id"];
        this->id = lpEv["id"];
    }
    this->is_chat = uint32_t(this->from_id) != this->peer_id; // явный каст: сравнение сохраняет поведение для from_id < 0
    if (!lpEv["attachments"].is_null())
    {
        for (auto attach : lpEv["attachments"])
            this->docs.emplace_back(Doc(attach));
    }
}

Event::Event(const Event& e): net(e.net), vk(e.vk)
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
    this->event_id = e.event_id;
    this->keyboard = e.keyboard;
    this->user = e.user;
    this->docs = e.docs;
    this->fwds = e.fwds;
}

json Event::send()
{
    table_t param;
    if (this->type.find("message_") != this->type.npos)
    {
        param["peer_id"] = std::to_string(this->peer_id);
        param["random_id"] = std::to_string(this->random_id);
        param["message"] = this->msg;
        param["disable_mentions"] = "1";
        if (!this->keyboard.empty())
            param["keyboard"] = this->keyboard;
        for (auto doc : this->docs)
            param["attachment"] += doc.get() + ",";
        return this->vk.send("messages.send", param);
    }
    if (this->type.find("wall_post_") != this->type.npos)
    {
        param["post_id"] = std::to_string(this->id);
        param["message"] = this->msg;
        param["owner_id"] = std::to_string(this->peer_id);
        for (auto doc : this->docs)
            param["attachment"] += doc.get() + ",";
        return this->vk.send("wall.createComment", param);
    }
    return NULL;
}

std::string Event::pysend()
{
    return this->send().dump(4);
}

Event Event::getOut()
{
    Event event(this->net, this->vk);
    event.type = this->type;
    event.from_id = this->from_id;
    event.peer_id = this->peer_id;
    event.id = this->id;
    event.random_id = this->random_id;
    event.event_id = this->event_id;
    event.user = this->user;
    return event;
}
