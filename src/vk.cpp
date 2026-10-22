#include "vk.h"
#include <stdexcept>

#define ver "5.92"

#include <iostream>

using namespace std;

Vk::Vk(Net& n)
    : net(n)
{
    conf.lock.lock();
    json& c = conf.get();
    if (!c["token"].is_null())
        this->token = c["token"];
    if (!c["user_token"].is_null())
        this->user_token = c["user_token"];
    conf.lock.unlock();
}

void Vk::validate()
{
    if (this->token.empty() || this->user_token.empty())
        throw std::runtime_error("Vk: не заданы token и/или user_token в config.json");
    json resp = this->send("groups.getTokenPermissions");
    if (resp["response"].is_null())
    {
        conf.lock.lock();
        conf.get()["token"] = NULL;
        conf.lock.unlock();
        conf.save();
        throw std::runtime_error("Vk: токен группы не прошёл groups.getTokenPermissions (проверьте token в config.json)");
    }
}

#include "timer.h"
json Vk::send(string method, table_t args, bool user)
{
    if (user)
        args["access_token"] = this->user_token;
    else
        args["access_token"] = this->token;
    if (args.find("v") == args.cend())
        args["v"] = ver;
    json buff = json::parse(this->net.send("https://api.vk.com/method/" + method, args));
    if(buff["error"]["error_code"].is_number())
        switch(buff["error"]["error_code"].get<int>())
        {
        case 9: // исправление пропуска при флуде
        case 10: // тоже самое, просто вк зараза
            timer::sleep(3600000);
            return this->send(method, args, user);
            break;
        case 6: // ограничение запросов в секунду
            timer::sleep(1000);
            return this->send(method, args, user);
            break;
        default:
            cout << endl << "VK ERROR:" << endl << buff["error"].dump(4) << endl;
            break;
        }
    return buff;
}


