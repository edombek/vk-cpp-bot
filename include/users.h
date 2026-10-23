#pragma once

#include "common.h"
#include "vk.h"

namespace users
{
typedef struct user_t
{
    std::string name;
    uint64_t id = 0;
    uint8_t acess = 0;
    uint64_t msgs = 0;
} user;
json newUser(uint64_t id, Vk& vk);
user getUser(uint64_t id, Vk& vk, bool inc = true); // inc = false — чтение без увеличения счётчика msgs
void changeUser(uint64_t id, const user& edited);
}
