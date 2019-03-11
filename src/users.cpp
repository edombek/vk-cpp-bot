#include "users.h"
#include "bd.h"
#include <iostream>

json users::newUser(uint32_t id, Vk* vk)
{
    json u = {};
    u["acess"] = 1;
    u["msgs"] = 0;
    u["name"] = vk->send("users.get", { { "user_ids", std::to_string(id) } })["response"][0]["first_name"];
    return u;
}

users::user users::getUser(uint32_t id, Vk* vk)
{
    users::user u;
    bd.lock.lock();
    json& us = bd.get()["users"];
    std::string strid = std::to_string(id);
    if (us[strid].is_null())
        us[strid] = users::newUser(id, vk);
    u.id = id;
    u.name = us[strid]["name"];
    u.acess = us[strid]["acess"];
    u.msgs = us[strid]["msgs"];
    us[strid]["msgs"] = u.msgs + 1;
    bd.lock.unlock();
    return u;
}

void users::changeUser(uint32_t id, users::user edited)
{
    bd.lock.lock();
    json& us = bd.get()["users"];
    std::string strid = std::to_string(id);
    us[strid]["name"] = edited.name;
    us[strid]["acess"] = edited.acess;
    us[strid]["msgs"] = edited.msgs;
    us[strid]["msgs"] = edited.msgs;
    bd.lock.unlock();
}
