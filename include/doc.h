#ifndef DOC_H_INCLUDED
#define DOC_H_INCLUDED
#include "common.h"

class Doc {
public:
    Doc(json lpDoc);
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
