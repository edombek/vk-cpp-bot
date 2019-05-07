#include "vk.h"

#define ver "5.92"

using namespace std;

Vk::Vk(Net& n)
    : net(n)
{
    json& c = conf.get();
    if (c["token"].is_null() || c["user_token"].is_null())
    {
        conf.save();
        throw;
    }
    this->token = c["token"];
    this->user_token = c["user_token"];
    json resp = this->send("groups.getTokenPermissions");
    if (resp["response"].is_null())
    {
        c["token"] = NULL;
        conf.save();
        throw;
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
    {
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
            break;
        }
    }
    return buff;
}
