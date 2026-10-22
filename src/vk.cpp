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
static const int vkFloodRetries = 3; // повторов при флуд-ошибке VK (была бесконечная рекурсия)
json Vk::send(const string& method, table_t args, bool user)
{
    if (user)
        args["access_token"] = this->user_token;
    else
        args["access_token"] = this->token;
    if (args.find("v") == args.cend())
        args["v"] = ver;
    for (int attempt = 0;; attempt++)
    {
        json buff = json::parse(this->net.send("https://api.vk.com/method/" + method, args));
        if (!buff["error"]["error_code"].is_number())
            return buff;
        int code = buff["error"]["error_code"].get<int>();
        bool retry = false;
        int delayMs = 1000;
        if (code == 6 && attempt < 5) // ограничение запросов в секунду
            retry = true;
        else if (code == 9 || code == 10) // флуд-фильтр VK; раньше спали час и рекурсивно зацикливались
        {
            long long d = 5000; // запас, если request_delay не прислан
            if (buff["error"]["request_delay"].is_number())
                d = buff["error"]["request_delay"].get<long long>() * 1000; // VK сам указывает, сколько ждать (сек)
            delayMs = (int)(d > 60000 ? 60000 : d); // не спим дольше минуты
            retry = attempt < vkFloodRetries;
        }
        if (!retry)
        {
            cout << endl << "VK ERROR:" << endl << buff["error"].dump(4) << endl;
            return buff; // ошибки больше не плодят бесконечную рекурсию — вызывающий код сам решает
        }
        timer::sleep(delayMs);
    }
}


