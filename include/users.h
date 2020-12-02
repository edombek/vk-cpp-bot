#pragma once

#include "common.h"
#include "vk.h"

namespace users
{
typedef struct
{
    std::string name;
    uint32_t id;
    uint8_t acess;
    uint64_t msgs;
} user;
json newUser(uint32_t id, Vk& vk);
user getUser(uint32_t id, Vk& vk);
void changeUser(uint32_t id, user edited);
}
