#pragma once
#include "common.h"
#include "vk.h"

class Doc
{
public:
    Doc(json lpDoc = NULL);
    Doc(std::string jsonstr): Doc(json::parse(jsonstr)) {}
    bool uploadDoc(std::string filename, std::string& data, Net& net, Vk& vk, uint32_t peer_id, bool audio_message = false);
    bool uploadPhoto(std::string filename, std::string& data, Net& net, Vk& vk, uint32_t peer_id);
    std::string get() const;
    std::string type;
    int owner_id;
    int doc_id;
    std::string acess_key;
    std::string ext;
    std::string url;
    int timestamp;
    bool operator==(const Doc& d)
    {
        return (this->get()) == (d.get());
    }
};
