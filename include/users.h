#pragma once

#include "common.h"
#include "vk.h"

namespace users
{
typedef struct user_t
{
    std::string name;
    // знаковый: отрицательные id — сообщества (комментарии стены от имени страницы)
    int64_t id = 0;
    uint8_t acess = 0;
    uint64_t msgs = 0;
} user;
json newUser(int64_t id, Vk& vk);
user getUser(int64_t id, Vk& vk, bool inc = true); // inc = false — чтение без увеличения счётчика msgs
void changeUser(int64_t id, const user& edited);
}
