#pragma once
#include "common.h"
#include "vk.h"

class Doc
{
public:
    Doc(json lpDoc = NULL);
    Doc(const std::string& jsonstr): Doc(json::parse(jsonstr)) {}
    bool uploadDoc(const std::string& filename, const std::string& data, Net& net, Vk& vk, uint32_t peer_id, bool audio_message = false);
    bool uploadPhoto(const std::string& filename, const std::string& data, Net& net, Vk& vk, uint32_t peer_id);
    std::string get() const;
    std::string type;
    int owner_id = 0;
    int doc_id = 0;
    std::string acess_key;
    std::string ext;
    std::string url;
    int timestamp = 0;
    bool operator==(const Doc& d) const
    {
        return (this->get()) == (d.get());
    }
};
