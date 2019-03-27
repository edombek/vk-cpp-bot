#ifndef DOC_H_INCLUDED
#define DOC_H_INCLUDED
#include "common.h"
#include "vk.h"

class Doc {
public:
    Doc(json lpDoc = NULL);
    bool uploadDoc(std::string filename, std::string& data, Net& net, Vk& vk, uint32_t peer_id, bool audio_message = false);
    bool uploadPhoto(std::string filename, std::string& data, Net& net, Vk& vk, uint32_t peer_id);
    std::string get();
    std::string type;
    int owner_id;
    int doc_id;
    std::string acess_key;
    std::string ext;
    std::string url;
    int timestamp;
};

#endif // DOC_H_INCLUDED
