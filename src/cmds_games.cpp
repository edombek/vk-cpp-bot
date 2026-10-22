// Игровые команды: /g (линксы 4×4), /bc (быки и коровы)
#include "cmds.h"
#include "game.h"
#include "keyboard.h"
#include "str.h"
#include <algorithm>
#include <cctype>
#include <mutex>
#include <random>

using namespace std;

map<uint32_t, game*> gameNew = {}; // новые игры в чате
map<uint32_t, map<uint32_t, game*>> gameUsers = {}; // все игры в [chat][user]
mutex gameL;

void cleanGames(game *t, uint32_t peer_id) // удаляем игры в каталогах пользователей и саму игру
{
    gameUsers[peer_id].erase(t->users_id[0]);
    if(t->users_id[1])
        gameUsers[peer_id].erase(t->users_id[1]);
    if(!gameUsers[peer_id].size())
        gameUsers.erase(peer_id);
    delete t;
}

void gameF(Event &eventIn, Event &eventOut, Worker *worker)
{
    if(!eventIn.is_chat)
    {
        eventOut.msg = "только для бесед!";
        return;
    }
    gameL.lock();
    game* t;

    if(findinmap(gameUsers, eventIn.peer_id) && findinmap(gameUsers[eventIn.peer_id], eventIn.user.id) && gameUsers[eventIn.peer_id][eventIn.user.id]->users_id[1])//игра готова?
        t = gameUsers[eventIn.peer_id][eventIn.user.id];
    else
    {
        if(findinmap(gameNew, eventIn.peer_id) && !gameNew[eventIn.peer_id]->users_id[1])//есть ли созданная в чате но без второго игрока?
        {
            if(gameNew[eventIn.peer_id]->users_id[0]==eventIn.user.id)
            {
                eventOut.msg+="нельзя создать игру с самим собой! Удаляем эту игру!";
                cleanGames(gameNew[eventIn.peer_id], eventIn.peer_id);
                gameNew.erase(eventIn.peer_id);
                gameL.unlock();
                return;
            }
            t=gameNew[eventIn.peer_id];
            t->users_id[1] = eventIn.user.id;
            gameUsers[eventIn.peer_id][eventIn.user.id] = t;
            gameNew.erase(eventIn.peer_id);
            goto printMap;
        }
        else  //создаём новую игру, ожидаем
        {
            t = new game;
            t->users_id[0] = eventIn.user.id;
            gameNew[eventIn.peer_id] = t;
            gameUsers[eventIn.peer_id][eventIn.user.id] = t;

            eventOut.msg += "игра создана, ожидаем второго игрока";
            gameL.unlock();
            return;
        }
    }

    if(eventIn.words.size() > 1 && eventIn.words[1] == "stop")
    {
        eventOut.msg+="игра остановлена!";
        cleanGames(t, eventIn.peer_id);
        gameL.unlock();
        return;
    }
    if(eventIn.user.id!=t->users_id[t->user])
    {
        eventOut.msg+="не твой ход!";
        gameL.unlock();
        return;
    }
    if(eventIn.words.size() < 3)
    {
        eventOut.msg+="кординату введи...";
        gameL.unlock();
        return;
    }

    t->gameUplevel(str::fromString(eventIn.words[1]), str::fromString(eventIn.words[2]));

printMap:
    eventOut.msg+="\n"+t->nexStep();
    eventOut.send();

    if(t->isWin()) //выйграл?
    {
        eventOut.msg="выйграл!";
        cleanGames(t, eventIn.peer_id);
        gameL.unlock();
        return;
    }

    eventOut.msg="Ходит @id" + to_string(t->users_id[t->user]);
    gameL.unlock();
}

string getNumber() // генерация случайного четырёхзначного числа без повторений цифр
{
    string nums = "1234567890";
    static mt19937 rng(random_device{}()); // один сеятель на процесс, потокобезопасная инициализация
    shuffle(nums.begin(), nums.end(), rng);
    if(nums[0]=='0')
        swap(nums[0], nums[9]);
    nums.resize(4);
    return nums;
}

map<int, string> randNumbers = {}; // все загаданные числа для каждого пользователя
map<uint32_t, string> bcDraft = {}; // текущий набор цифр при вводе кнопками
mutex bcL;

// клавиатура игры: набор цифр + действия над черновиком
static Keyboard bcKeyboard()
{
    Keyboard kbd(false, false);
    for (char d = '1'; d <= '9'; d++)
        kbd.button(string(1, d), "/bc num " + string(1, d));
    kbd.button("0", "/bc num 0");
    kbd.row();
    kbd.button("⌫ стереть", "/bc del")
        .button("✔ проверить", "/bc go", "positive")
        .button("✖ сброс", "/bc reset", "negative");
    return kbd;
}

void bc(Event &eventIn, Event &eventOut, Worker *worker)
{
    bcL.lock();

    goto _pass;
pass:
    bcL.unlock();
    return;
_pass:

    if(!findinmap(randNumbers, eventIn.user.id))//если игра не создана
    {
        randNumbers[eventIn.user.id] = getNumber();
        eventOut.msg+="\
цель данной игры отгадать загаданное компьютером число.\n\
Даются подсказки в виде двух значений.\n\
Коров -- числа совпадений цифр без совпадений их положения.\n\
Быков -- числа точных совпадений цифр.\n\
Ответ боту даётся как аргумент к этой команде или кнопками:";
        eventOut.keyboard = bcKeyboard().dump();
        goto pass;
    }

    // ввод с клавиатуры: "/bc num <цифра>", "/bc del", "/bc go", "/bc reset"
    string action = eventIn.words.size() > 1 ? str::low(eventIn.words[1]) : "";
    if (action == "num" || action == "del" || action == "go" || action == "reset")
    {
        string& draft = bcDraft[eventIn.user.id];
        if (action == "num")
        {
            if (eventIn.words.size() != 3 || eventIn.words[2].size() != 1 || !isdigit((unsigned char)eventIn.words[2][0]))
                goto pass;
            if (draft.size() < 4)
                draft += eventIn.words[2][0];
        }
        else if (action == "del")
        {
            if (draft.size())
                draft.pop_back();
        }
        else if (action == "reset")
            draft.clear();
        if (action != "go")
        {
            eventOut.msg += "ввод: " + (draft.size() ? draft : string("(пусто)")) + " [" + to_string(draft.size()) + "/4]";
            eventOut.keyboard = bcKeyboard().dump();
            goto pass;
        }
        if (!draft.size())
        {
            eventOut.msg += "нечего проверять — набери число.";
            eventOut.keyboard = bcKeyboard().dump();
            goto pass;
        }
        eventIn.words[1] = draft; // дальше как обычный ответ цифрой
        draft.clear();
    }

    if(eventIn.words.size() < 2)
    {
        eventOut.msg+="ты не указал четырёхзначное число.";
        eventOut.keyboard = bcKeyboard().dump();
        goto pass;
    }
    string userNumber = eventIn.words[1];
    if(!(userNumber.size() == 4 && str::fromString(userNumber) && userNumber[0] != '0')) // если это не четырёх значное число
    {
        eventOut.msg+="ты указал не четырёхзначное число.";
        eventOut.keyboard = bcKeyboard().dump();
        goto pass;
    }


    if(userNumber == randNumbers[eventIn.user.id])
    {
        eventOut.msg+="молодец!!! Ты отгадал число!";
        randNumbers.erase(eventIn.user.id);
        bcDraft.erase(eventIn.user.id);
        goto pass;
    }

    string randNumber = randNumbers[eventIn.user.id];

    uint8_t c = 0; // коровы
    uint8_t b = 0; // быки
    for(uint8_t ri = 0; ri < 4; ri++)
    {
        if(randNumber[ri] == userNumber[ri])// если бык
        {
            b++;
            continue;
        }
        for(uint8_t ui = 0; ui < 4; ui++)
            if(randNumber[ri] == userNumber[ui]) // если корова
            {
                c++;
                break;
            }
    }

    eventOut.msg = "найдено:\n  Коров: " + to_string(c) + "\n  Быков: " + to_string(b);
    eventOut.keyboard = bcKeyboard().dump();
    bcL.unlock();
}
