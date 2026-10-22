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
    Vk(Net& n); // только загрузка токенов из conf; сеть не трогает
    void validate(); // проверка боевого токена группы (сетапит Lp на старте); бросает runtime_error при ошибке
    json send(std::string method, table_t args = {}, bool user = false);
};
