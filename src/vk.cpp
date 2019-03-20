#include "vk.h"

#define ver "5.92"

using namespace std;
#include <iostream>

Vk::Vk(Net* n)
{
    this->net = n;
    json& c = conf.get();
    if (c["token"].is_null() || c["user_token"].is_null()) {
        conf.save();
        throw;
    }
    this->token = c["token"];
    this->user_token = c["user_token"];
    json resp = this->send("groups.getTokenPermissions");
    if (resp["response"].is_null()) {
        cout << resp.dump(4);
        c["token"] = NULL;
        conf.save();
        throw;
    }
}

json Vk::send(string method, table_t args, bool user)
{
    if (user)
        args["access_token"] = this->user_token;
    else
        args["access_token"] = this->token;
    if (args.find("v") == args.cend())
        args["v"] = ver;
    this->net->send("https://api.vk.com/method/" + method, args);
    return json::parse(this->net->buffer);
}
