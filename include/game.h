#pragma once

#include "common.h"
#define sizeGame 4

class game
{
private:
    uint8_t map[sizeGame][sizeGame][2]; // [x][y][user/level]
    uint32_t count;
public:
    uint8_t user;
    uint32_t users_id[2]; // ид игроков
    game(); // инициализация
    void gameUplevel(uint8_t x, uint8_t y, bool replase = false, uint8_t step = sizeGame); //повышаем уровень клетки
    bool isWin(); //выйграл ли?
    std::string nexStep(); //рисуем карту и переключаемся на следующий шаг
};
