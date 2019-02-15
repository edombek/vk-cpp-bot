#include "common.h"

Doc::Doc(json lpDoc)
{
    this->type = lpDoc["type"];
    if (this->type != "doc" && this->type != "photo" && this->type != "audio" && this->type != "video" && this->type != "audio_message")
        return;
    if (this->type == "doc" || this->type == "photo" || this->type == "audio" || this->type == "video")
        this->timestamp = lpDoc[this->type]["date"];
    if (this->type == "doc")
        this->ext = lpDoc[this->type]["ext"];
    if (this->type != "photo")
        if (this->type == "audio_message") {
            this->url = lpDoc[this->type]["link_mp3"];
            this->ext = "mp3";
        } else if (this->type != "video")
            this->url = lpDoc[this->type]["url"];
        else {
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
