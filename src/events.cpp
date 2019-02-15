#include "common.h"

Event::Event(json lpEv)
{
    if (lpEv == NULL)
        return;
    this->type = lpEv["type"];
    lpEv = lpEv["object"];
    this->msg = lpEv["text"];
    this->from_id = lpEv["from_id"];
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
        this->post_id = lpEv["id"];
    }
    this->is_chat = !this->post_id && this->from_id == this->peer_id;
    if (!lpEv["attachments"].is_null()) {
        for (auto attach : lpEv["attachments"])
            this->docs.push_back(new Doc(attach));
    }
}

Event::~Event()
{
    for (auto fwd : this->fwds)
        delete fwd;
    for (auto doc : docs)
        delete doc;
}
