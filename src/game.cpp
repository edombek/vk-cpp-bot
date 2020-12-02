#include "game.h"

using namespace std;
#define nullsim "⭕"

const string nums[] = { "0⃣", "1⃣", "2⃣", "3⃣", "4⃣", "5⃣", "6⃣", "7⃣", "8⃣", "9⃣" };
const string levels[2][3] =
{
    { "⚪", { (char)4294967280, (char)4294967199, (char)4294967188, (char)4294967224 }, { (char)4294967280, (char)4294967199, (char)4294967188, (char)4294967222 } },
    { "⚪", { (char)4294967280, (char)4294967199, (char)4294967188, (char)4294967225 }, { (char)4294967280, (char)4294967199, (char)4294967188, (char)4294967223 } }
};

game::game()
{
    this->user = 1;
    this->users_id[0] = 0;
    this->users_id[1] = 0;
    this->count = 0;
    for (int y = 0; y < sizeGame; y++)
        for (int x = 0; x < sizeGame; x++)
        {
            this->map[y][x][0] = 0;
            this->map[y][x][1] = 0;
        }
    this->map[0][0][0] = 0;
    this->map[0][0][1] = 1;
    this->map[sizeGame - 1][sizeGame - 1][0] = 1;
    this->map[sizeGame - 1][sizeGame - 1][1] = 1;
}

void game::gameUplevel(uint8_t x, uint8_t y, bool replase, uint8_t step)
{
    if (x >= sizeGame || y >= sizeGame || x < 0 || y < 0 || step == 0)
        return;
    if (this->map[x][y][1] >= 2 && (replase || this->map[x][y][0] == this->user)) //если она взрывается
    {
        this->map[x][y][1] = 0;
        this->map[x][y][0] = 0;
        gameUplevel(x, y + 1, true, step - 1);
        gameUplevel(x + 1, y, true, step - 1);
        gameUplevel(x, y - 1, true, step - 1);
        gameUplevel(x - 1, y, true, step - 1);
    }
    else
    {
        if (replase || this->map[x][y][0] == this->user || this->map[x][y][1] == 0)
        {
            this->map[x][y][1]++;
            this->map[x][y][0] = this->user;
        }
    }
}

bool game::isWin()
{
    bool s = true;
    for (int y = 0; y < sizeGame; y++)
        for (int x = 0; x < sizeGame; x++)
            if (this->map[x][y][0] == this->user && this->map[x][y][1] != 0)
                s = false;
    return s;
}

string game::nexStep()
{
    this->count++;
    this->user = (this->user + 1) % 2;
    string out = nullsim;
    for (int i = 0; i < sizeGame; i++)
        out += nums[i];
    out += "\n";
    for (int y = 0; y < sizeGame; y++)
    {
        out += nums[y];
        for (int x = 0; x < sizeGame; x++)
            out += levels[this->map[x][y][0]][this->map[x][y][1]];
        out += "\n";
    }
    return out;
}
