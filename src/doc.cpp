#include "doc.h"
#include "common.h"
#include "str.h"

Doc::Doc(json lpDoc)
{
    if (lpDoc == NULL || lpDoc.empty())
        return;
    if (lpDoc["type"].is_string())
        this->type = lpDoc["type"];
    else
        // wall_reply_new: одиночное «attachment» без поля type — выводим тип из ключа вложения
        for (auto it = lpDoc.begin(); it != lpDoc.end(); ++it)
            if (it.value().is_object())
            {
                this->type = it.key();
                break;
            }
    if (this->type == "doc")
    {
        lpDoc = lpDoc["doc"];
        this->doc_id = lpDoc["id"];
        this->owner_id = lpDoc["owner_id"];
        if (lpDoc["access_key"].is_string())
            this->acess_key = lpDoc["access_key"];
        this->ext = lpDoc["ext"];
        if (lpDoc["date"].is_number())
            this->timestamp = lpDoc["date"];
        if (lpDoc["url"].is_string())
            this->url = lpDoc["url"];
    }
    else if (this->type == "photo")
    {
        lpDoc = lpDoc["photo"];
        this->doc_id = lpDoc["id"];
        this->owner_id = lpDoc["owner_id"];
        if (lpDoc["access_key"].is_string())
            this->acess_key = lpDoc["access_key"];
        if (lpDoc["date"].is_number())
            this->timestamp = lpDoc["date"];
        int maxS = 0;
        if (lpDoc["sizes"].is_array())
            for (auto s : lpDoc["sizes"])
                if (s["width"].is_number() && s["height"].is_number() && s["url"].is_string() &&
                    maxS < s["width"].get<int>() * s["height"].get<int>())
                {
                    maxS = s["width"].get<int>() * s["height"].get<int>();
                    this->url = s["url"].get<std::string>();
                }
        if (!this->url.empty()) // пустой url без sizes иначе читал бы words.end()-1 (UB)
            this->ext = *(str::words(this->url, '.').end() - 1);
    }
    else if (this->type == "video")
    {
        lpDoc = lpDoc["video"];
        this->doc_id = lpDoc["id"];
        this->owner_id = lpDoc["owner_id"];
        if (lpDoc["access_key"].is_string())
            this->acess_key = lpDoc["access_key"];
        if (lpDoc["date"].is_number())
            this->timestamp = lpDoc["date"];
    }
    else if (this->type == "graffiti")
    {
        lpDoc = lpDoc["graffiti"];
        this->doc_id = lpDoc["id"];
        this->owner_id = lpDoc["owner_id"];
        if (lpDoc["access_key"].is_string())
            this->acess_key = lpDoc["access_key"];
        if (lpDoc["url"].is_string())
            this->url = lpDoc["url"];
        this->ext = "png";
    }
    else if (this->type == "music")
    {
        lpDoc = lpDoc["music"];
        this->doc_id = lpDoc["id"];
        this->owner_id = lpDoc["owner_id"];
        if (lpDoc["access_key"].is_string())
            this->acess_key = lpDoc["access_key"];
        this->ext = "mp3";
        if (lpDoc["date"].is_number())
            this->timestamp = lpDoc["date"];
        if (lpDoc["url"].is_string())
            this->url = lpDoc["url"];
    }
    else if (this->type == "audio_message")
    {
        lpDoc = lpDoc["audio_message"];
        this->doc_id = lpDoc["id"];
        this->owner_id = lpDoc["owner_id"];
        if (lpDoc["access_key"].is_string())
            this->acess_key = lpDoc["access_key"];
        this->ext = "mp3";
        if (lpDoc["link_mp3"].is_string())
            this->url = lpDoc["link_mp3"];
    }
}

bool Doc::uploadDoc(const std::string& filename, const std::string& data, Net& net, Vk& vk, int64_t peer_id, bool audio_message)
{
    table_t params;
    if (peer_id)
    {
        params["peer_id"] = std::to_string(peer_id);
        if (audio_message)
            params["type"] = "audio_message";
    }
    else
        return false;
    json resp = vk.send("docs.getMessagesUploadServer", params);
    if (resp["response"]["upload_url"].is_null())
        return false;
    resp = json::parse(net.upload(resp["response"]["upload_url"], filename, data), nullptr, false);
    if (resp.is_discarded() || !resp["file"].is_string() || resp["file"].get<std::string>().empty())
        return false;
    params =
    {
        { "file", resp["file"] }
    };
    resp = vk.send("docs.save", params);
    if (resp["response"].is_null())
        return false;
    resp = resp["response"];
    this->type = resp["type"];
    if (this->type == "doc")
    {
        resp = resp["doc"];
        this->doc_id = resp["id"];
        this->owner_id = resp["owner_id"];
        if (resp["access_key"].is_string())
            this->acess_key = resp["access_key"];
        this->ext = resp["ext"];
        this->timestamp = resp["date"];
        if (resp["url"].is_string()) // docs.save обычно возвращает doc БЕЗ url (нужен docs.get) — не ронять присваивание
            this->url = resp["url"];
    }
    else if (this->type == "graffiti")
    {
        resp = resp["graffiti"];
        this->doc_id = resp["id"];
        this->owner_id = resp["owner_id"];
        if (resp["access_key"].is_string())
            this->acess_key = resp["access_key"];
        if (resp["url"].is_string())
            this->url = resp["url"];
        this->ext = "png";
    }
    else if (this->type == "audio_message")
    {
        resp = resp["audio_message"];
        this->doc_id = resp["id"];
        this->owner_id = resp["owner_id"];
        if (resp["access_key"].is_string())
            this->acess_key = resp["access_key"];
        this->ext = "mp3";
        if (resp["link_mp3"].is_string())
            this->url = resp["link_mp3"];
    }
    else
        return false;
    return true;
}

bool Doc::uploadPhoto(const std::string& filename, const std::string& data, Net& net, Vk& vk, int64_t peer_id)
{
    table_t params;
    if (peer_id)
        params["peer_id"] = std::to_string(peer_id);
    else
        return false;
    // 2026: upload-сервер отдаёт photo в новом формате (JSON-строка «[{...}]») —
    // saveMessagesPhoto принимает её как есть; иногда же photo приходит пустым
    // (живой случай: /neon в личке -> «photo is undefined») — повторяем попытку,
    // пустой токен в saveMessagesPhoto не отправляем
    for (int attempt = 0; attempt < 2; attempt++)
    {
        json resp = vk.send("photos.getMessagesUploadServer", params);
        if (resp["response"]["upload_url"].is_null())
            return false;
        // VK требует поле «photo» (не «file»); lenient: upload-сервер может ответить не-JSON
        // (HTML-ошибка при большом файле) — раньше json::parse бросал и команда молчала (живой случай)
        resp = json::parse(net.upload(resp["response"]["upload_url"], filename, data, "photo"), nullptr, false);
        if (resp.is_discarded() || !resp["server"].is_number() || !resp["photo"].is_string() ||
            resp["photo"].get<std::string>().empty() || !resp["hash"].is_string())
            continue;
        params =
        {
            { "server", std::to_string(resp["server"].get<int>()) },
            { "photo", resp["photo"] },
            { "hash", resp["hash"] }
        };
        resp = vk.send("photos.saveMessagesPhoto", params);
        if (resp["response"][0].is_null())
            return false;
        resp = resp["response"][0];
        this->type = "photo";
        this->doc_id = resp["id"];
        this->owner_id = resp["owner_id"];
        if (resp["access_key"].is_string())
            this->acess_key = resp["access_key"];
        this->timestamp = resp["date"];
        int maxS = 0;
        for (auto s : resp["sizes"])
            if (maxS < s["width"].get<int>() * s["height"].get<int>())
            {
                maxS = s["width"].get<int>() * s["height"].get<int>();
                this->url = s["url"];
            }
        if (!this->url.empty()) // пустой url без sizes иначе читал бы words.end()-1 (UB)
            this->ext = *(str::words(this->url, '.').end() - 1);
        return true;
    }
    return false;
}

std::string Doc::get() const
{
    if (this->acess_key.size())
        return this->type + std::to_string(this->owner_id) + "_" + std::to_string(this->doc_id) + "_" + this->acess_key;
    return this->type + std::to_string(this->owner_id) + "_" + std::to_string(this->doc_id);
}
