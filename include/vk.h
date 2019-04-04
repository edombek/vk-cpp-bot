#pragma once
#include "bd.h"
#include "common.h"
#include "net.h"

class Vk
{
private:
    std::string token;
    std::string user_token;
    Net& net;

public:
    Vk(Net& n);
    json send(std::string method, table_t args = {}, bool user = false);
};
