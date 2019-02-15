#include "vk.h"

#define ver "5.92"

using namespace std;
#include <iostream>

Vk::Vk(Net* n)
{
    if (n == NULL) {
        this->n = true;
        this->net = new Net;
    } else {
        this->n = false;
        this->net = n;
    }
    json& c = conf.get();
    if (c["token"].is_null()) {
        conf.save();
        throw;
    }
    this->token = c["token"];
    json resp = this->send("groups.getTokenPermissions");
    if (resp["response"].is_null()) {
        cout << resp.dump(4);
        c["token"] = NULL;
        conf.save();
        throw;
    }
}

Vk::~Vk()
{
    if (this->n)
        delete this->net;
}

json Vk::send(string method, table_t args)
{
    args["access_token"] = this->token;
    if (args.find("v") == args.cend())
        args["v"] = ver;
    return json::parse(this->net->send("https://api.vk.com/method/" + method, args));
}
