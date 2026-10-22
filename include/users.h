#pragma once

#include "common.h"
#include "vk.h"

namespace users
{
typedef struct user_t
{
    std::string name;
    uint32_t id = 0;
    uint8_t acess = 0;
    uint64_t msgs = 0;
} user;
json newUser(uint32_t id, Vk& vk);
user getUser(uint32_t id, Vk& vk, bool inc = true); // inc = false — чтение без увеличения счётчика msgs
void changeUser(uint32_t id, user edited);
}
