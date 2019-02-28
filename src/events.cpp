#include "events.h"
#include "common.h"
#include "str.h"
Event::Event(json lpEv)
{
    this->copied = false;
    if (lpEv == NULL)
        return;
    this->type = lpEv["type"];
    lpEv = lpEv["object"];
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
    if (this->type.find("message_") != this->type.npos) {
        this->peer_id = lpEv["peer_id"];
        if (!lpEv["reply_message"].is_null())
            this->fwds.push_back(new Event({ { "type", this->type }, { "object", lpEv["reply_message"] } }));
        if (!lpEv["fwd_messages"].is_null())
            for (auto fwd : lpEv["fwd_messages"])
                this->fwds.push_back(new Event({ { "type", this->type }, { "object", fwd } }));
    }
    if (this->type.find("wall_post_") != this->type.npos) {
        this->peer_id = lpEv["owner_id"];
        this->id = lpEv["id"];
    }
    this->is_chat = !this->id && this->from_id == this->peer_id;
    if (!lpEv["attachments"].is_null()) {
        for (auto attach : lpEv["attachments"])
            this->docs.push_back(new Doc(attach));
    }
}

Event::~Event()
{
    if (this->copied)
        return;
    for (auto fwd : this->fwds)
        delete fwd;
    for (auto doc : docs) {
        if (doc)
            delete doc;
    }
}

void Event::setNet(Net* n, Vk* v)
{
    this->net = n;
    this->vk = v;
}

json Event::send()
{
    table_t param;
    if (this->type.find("message_") != this->type.npos) {
        param["peer_id"] = std::to_string(this->peer_id);
        param["random_id"] = std::to_string(this->random_id);
        param["message"] = this->msg;
        for (auto doc : this->docs)
            if (doc)
                param["attachment"] += doc->get() + ",";
        return this->vk->send("messages.send", param);
    }
    if (this->type.find("wall_post_") != this->type.npos) {
        param["post_id"] = std::to_string(this->id);
        param["message"] = this->msg;
        param["owner_id"] = std::to_string(this->peer_id);
        for (auto doc : this->docs)
            param["attachment"] += doc->get() + ",";
        return this->vk->send("wall.createComment", param);
    }
    return NULL;
}

Event* Event::copy()
{
    Event* event = new Event;
    *event = *this;
    this->copied = true;
    return event;
}

Event* Event::getOut()
{
    Event* event = new Event;
    event->type = this->type;
    event->from_id = this->from_id;
    event->peer_id = this->peer_id;
    event->id = this->id;
    event->random_id = this->random_id;
    event->user = this->user;
    return event;
}
