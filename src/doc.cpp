#include "doc.h"
#include "common.h"
#include "str.h"

Doc::Doc(json lpDoc)
{
    this->setDoc(lpDoc);
}
#include <iostream>
Doc::Doc(std::string type, std::string peer_id, std::string filename, std::string& data, Net* net, Vk* vk)
{
    json res;
    table_t params = {
        { "peer_id", peer_id },
        { "type", type }
    };
    if (type != "audio_message", type != "graffiti")
        res = vk->send(type + "s.getMessagesUploadServer", params)["response"];
    else
        res = vk->send("docs.getMessagesUploadServer", params)["response"];
    net->upload(res["upload_url"], filename, data);
    if (net->buffer == "" || str::at(net->buffer, "504 Gateway Time-out")) {
        return;
    }
    res = json::parse(net->buffer);
    if (!res["error"].is_null())
        return;
    params = {};
    if (type == "photo") {
        params["server"] = std::to_string(res["server"].get<int>());
        params["photo"] = res["photo"];
        params["hash"] = res["hash"];
        res = vk->send("photos.saveMessagesPhoto", params);
    } else if (type == "doc" || type == "audio_message" || type == "graffiti") {
        params["file"] = res["file"];
        res = vk->send("docs.save", params);
    }

    Doc::setDoc(res["response"]);
}

void Doc::setDoc(json lpDoc)
{
    this->type = lpDoc["type"];
    if (this->type != "doc" && this->type != "photo" && this->type != "audio" && this->type != "video" && this->type != "audio_message")
        return;
    if (this->type == "doc" || this->type == "photo" || this->type == "audio" || this->type == "video")
        this->timestamp = lpDoc[this->type]["date"];
    if (this->type == "doc")
        this->ext = lpDoc[this->type]["ext"];
    if (this->type != "photo") {
        if (this->type == "audio_message") {
            this->url = lpDoc[this->type]["link_mp3"];
            this->ext = "mp3";
        } else if (this->type != "video")
            this->url = lpDoc[this->type]["url"];
    } else {
        uint32_t maxS = 0;
        for (auto s : lpDoc[this->type]["sizes"])
            if (maxS < s["width"].get<int>() * s["height"].get<int>()) {
                maxS = s["width"].get<int>() * s["height"].get<int>();
                this->url = s["url"];
            }
    }
    if (lpDoc[this->type]["access_key"].is_string())
        this->acess_key = lpDoc[this->type]["access_key"];
    this->doc_id = lpDoc[this->type]["id"];
    this->owner_id = lpDoc[this->type]["owner_id"];
}

std::string Doc::get()
{
    if (this->acess_key.size())
        return this->type + std::to_string(this->owner_id) + "_" + std::to_string(this->doc_id) + "_" + this->acess_key;
    return this->type + std::to_string(this->owner_id) + "_" + std::to_string(this->doc_id);
}
