#include "doc.h"
#include "common.h"
#include "str.h"

Doc::Doc(json lpDoc)
{
    if (lpDoc == NULL)
        return;
    this->type = lpDoc["type"];
    if (this->type == "doc") {
        lpDoc = lpDoc["doc"];
        this->doc_id = lpDoc["id"];
        this->owner_id = lpDoc["owner_id"];
        if (lpDoc["access_key"].is_string())
            this->acess_key = lpDoc["access_key"];
        this->ext = lpDoc["ext"];
        this->timestamp = lpDoc["date"];
        this->url = lpDoc["url"];
    } else if (this->type == "photo") {
        lpDoc = lpDoc["photo"];
        this->doc_id = lpDoc["id"];
        this->owner_id = lpDoc["owner_id"];
        if (lpDoc["access_key"].is_string())
            this->acess_key = lpDoc["access_key"];
        this->timestamp = lpDoc["date"];
        uint32_t maxS = 0;
        for (auto s : lpDoc["sizes"])
            if (maxS < s["width"].get<int>() * s["height"].get<int>()) {
                maxS = s["width"].get<int>() * s["height"].get<int>();
                this->url = s["url"];
            }
        this->ext = *(str::words(this->url, '.').end() - 1);
    } else if (this->type == "video") {
        lpDoc = lpDoc["video"];
        this->doc_id = lpDoc["id"];
        this->owner_id = lpDoc["owner_id"];
        if (lpDoc["access_key"].is_string())
            this->acess_key = lpDoc["access_key"];
        this->timestamp = lpDoc["date"];
    } else if (this->type == "graffiti") {
        lpDoc = lpDoc["graffiti"];
        this->doc_id = lpDoc["id"];
        this->owner_id = lpDoc["owner_id"];
        if (lpDoc["access_key"].is_string())
            this->acess_key = lpDoc["access_key"];
        this->url = lpDoc["url"];
        this->ext = "png";
    } else if (this->type == "music") {
        lpDoc = lpDoc["music"];
        this->doc_id = lpDoc["id"];
        this->owner_id = lpDoc["owner_id"];
        if (lpDoc["access_key"].is_string())
            this->acess_key = lpDoc["access_key"];
        this->ext = "mp3";
        this->timestamp = lpDoc["date"];
        this->url = lpDoc["url"];
    } else if (this->type == "audio_message") {
        lpDoc = lpDoc["audio_message"];
        this->doc_id = lpDoc["id"];
        this->owner_id = lpDoc["owner_id"];
        if (lpDoc["access_key"].is_string())
            this->acess_key = lpDoc["access_key"];
        this->ext = "mp3";
        this->url = lpDoc["link_mp3"];
    }
}

bool Doc::uploadDoc(std::string filename, std::string& data, Net* net, Vk* vk, uint32_t peer_id, bool audio_message)
{
    table_t params;
    if (peer_id) {
        params["peer_id"] = std::to_string(peer_id);
        if (audio_message)
            params["type"] = "audio_message";
    } else
        return false;
    json resp = vk->send("docs.getMessagesUploadServer", params);
    if (resp["response"]["upload_url"].is_null())
        return false;
    net->upload(resp["response"]["upload_url"], filename, data);
    resp = json::parse(net->buffer);
    if (resp["file"].is_null())
        return false;
    params = {
        { "file", resp["file"] }
    };
    resp = vk->send("docs.save", params);
    if (resp["response"].is_null())
        return false;
    resp = resp["response"];
    this->type = resp["type"];
    if (this->type == "doc") {
        resp = resp["doc"];
        this->doc_id = resp["id"];
        this->owner_id = resp["owner_id"];
        if (resp["access_key"].is_string())
            this->acess_key = resp["access_key"];
        this->ext = resp["ext"];
        this->timestamp = resp["date"];
        this->url = resp["url"];
    } else if (this->type == "graffiti") {
        resp = resp["graffiti"];
        this->doc_id = resp["id"];
        this->owner_id = resp["owner_id"];
        if (resp["access_key"].is_string())
            this->acess_key = resp["access_key"];
        this->url = resp["url"];
        this->ext = "png";
    } else if (this->type == "audio_message") {
        resp = resp["audio_message"];
        this->doc_id = resp["id"];
        this->owner_id = resp["owner_id"];
        if (resp["access_key"].is_string())
            this->acess_key = resp["access_key"];
        this->ext = "mp3";
        this->url = resp["link_mp3"];
    } else
        return false;
    return true;
}

bool Doc::uploadPhoto(std::string filename, std::string& data, Net* net, Vk* vk, uint32_t peer_id)
{
    table_t params;
    if (peer_id)
        params["peer_id"] = std::to_string(peer_id);
    else
        return false;
    json resp = vk->send("photos.getMessagesUploadServer", params);
    if (resp["response"]["upload_url"].is_null())
        return false;
    net->upload(resp["response"]["upload_url"], filename, data);
    resp = json::parse(net->buffer);
    if (resp["server"].is_null())
        return false;
    params = {
        { "server", std::to_string(resp["server"].get<int>()) },
        { "photo", resp["photo"] },
        { "hash", resp["hash"] }
    };
    resp = vk->send("photos.saveMessagesPhoto", params);
    if (resp["response"][0].is_null())
        return false;
    resp = resp["response"][0];
    this->type = "photo";
    this->doc_id = resp["id"];
    this->owner_id = resp["owner_id"];
    if (resp["access_key"].is_string())
        this->acess_key = resp["access_key"];
    this->timestamp = resp["date"];
    uint32_t maxS = 0;
    for (auto s : resp["sizes"])
        if (maxS < s["width"].get<int>() * s["height"].get<int>()) {
            maxS = s["width"].get<int>() * s["height"].get<int>();
            this->url = s["url"];
        }
    this->ext = *(str::words(this->url, '.').end() - 1);
    return true;
}

std::string Doc::get()
{
    if (this->acess_key.size())
        return this->type + std::to_string(this->owner_id) + "_" + std::to_string(this->doc_id) + "_" + this->acess_key;
    return this->type + std::to_string(this->owner_id) + "_" + std::to_string(this->doc_id);
}
