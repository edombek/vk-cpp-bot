#ifndef VK_H_INCLUDED
#define VK_H_INCLUDED
#include "bd.h"
#include "common.h"
#include "net.h"

class Vk {
private:
    std::string token;
    Net* net;
    bool n;

public:
    Vk(Net* n = NULL);
    ~Vk();
    json send(std::string method, table_t args = {});
};

#endif // VK_H_INCLUDED
