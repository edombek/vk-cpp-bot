// Игровые команды: /g (линксы 4×4), /bc (быки и коровы)
#include "cmds.h"
#include "game.h"
#include "keyboard.h"
#include "str.h"
#include <algorithm>
#include <cctype>
#include <memory>
#include <mutex>
#include <random>

using namespace std;

map<uint32_t, shared_ptr<game>> gameNew = {}; // новые игры в чате
map<uint32_t, map<uint32_t, shared_ptr<game>>> gameUsers = {}; // все игры в [chat][user]
mutex gameL;

// убираем игру из каталогов пользователей; сама игра живёт в shared_ptr
// и уничтожается вместе с последней ссылкой (бывший delete)
static void cleanGames(const shared_ptr<game>& t, uint32_t peer_id)
{
    gameUsers[peer_id].erase(t->users_id[0]);
    if (t->users_id[1])
        gameUsers[peer_id].erase(t->users_id[1]);
    if (!gameUsers[peer_id].size())
        gameUsers.erase(peer_id);
}

void gameF(Event &eventIn, Event &eventOut, Worker *worker)
{
    if (!eventIn.is_chat)
    {
        eventOut.msg = "только для бесед!";
        return;
    }
    lock_guard<mutex> guard(gameL); // разблокировка на любом выходе из функции
    shared_ptr<game> t;
    bool justJoined = false; // присоединились к игре — сразу выводим карту

    if (findinmap(gameUsers, eventIn.peer_id) && findinmap(gameUsers[eventIn.peer_id], eventIn.user.id)
        && gameUsers[eventIn.peer_id][eventIn.user.id]->users_id[1]) // игра готова?
        t = gameUsers[eventIn.peer_id][eventIn.user.id];
    else if (findinmap(gameNew, eventIn.peer_id) && !gameNew[eventIn.peer_id]->users_id[1]) // есть созданная, но без второго игрока?
    {
        if (gameNew[eventIn.peer_id]->users_id[0] == eventIn.user.id)
        {
            eventOut.msg += "нельзя создать игру с самим собой! Удаляем эту игру!";
            cleanGames(gameNew[eventIn.peer_id], eventIn.peer_id);
            gameNew.erase(eventIn.peer_id);
            return;
        }
        t = gameNew[eventIn.peer_id];
        t->users_id[1] = eventIn.user.id;
        gameUsers[eventIn.peer_id][eventIn.user.id] = t;
        gameNew.erase(eventIn.peer_id);
        justJoined = true;
    }
    else // создаём новую игру, ожидаем второго игрока
    {
        t = make_shared<game>();
        t->users_id[0] = eventIn.user.id;
        gameNew[eventIn.peer_id] = t;
        gameUsers[eventIn.peer_id][eventIn.user.id] = t;

        eventOut.msg += "игра создана, ожидаем второго игрока";
        return;
    }

    if (!justJoined)
    {
        if (eventIn.words.size() > 1 && eventIn.words[1] == "stop")
        {
            eventOut.msg += "игра остановлена!";
            cleanGames(t, eventIn.peer_id);
            return;
        }
        if (eventIn.user.id != t->users_id[t->user])
        {
            eventOut.msg += "не твой ход!";
            return;
        }
        if (eventIn.words.size() < 3)
        {
            eventOut.msg += "кординату введи...";
            return;
        }
        t->gameUplevel(str::fromString(eventIn.words[1]), str::fromString(eventIn.words[2]));
    }

    // вывод карты после хода (бывшая метка printMap)
    eventOut.msg += "\n" + t->nexStep();
    eventOut.send();

    if (t->isWin()) // выиграл?
    {
        eventOut.msg = "выйграл!";
        cleanGames(t, eventIn.peer_id);
        return;
    }

    eventOut.msg = "Ходит @id" + to_string(t->users_id[t->user]);
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

map<uint32_t, string> randNumbers = {}; // все загаданные числа для каждого пользователя
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
    lock_guard<mutex> guard(bcL); // разблокировка на любом выходе из функции

    if (!findinmap(randNumbers, eventIn.user.id)) // если игра не создана
    {
        randNumbers[eventIn.user.id] = getNumber();
        eventOut.msg += "\
цель данной игры отгадать загаданное компьютером число.\n\
Даются подсказки в виде двух значений.\n\
Коров -- числа совпадений цифр без совпадений их положения.\n\
Быков -- числа точных совпадений цифр.\n\
Ответ боту даётся как аргумент к этой команде или кнопками:";
        eventOut.keyboard = bcKeyboard().dump();
        return;
    }

    // ввод с клавиатуры: "/bc num <цифра>", "/bc del", "/bc go", "/bc reset"
    string action = eventIn.words.size() > 1 ? str::low(eventIn.words[1]) : "";
    if (action == "num" || action == "del" || action == "go" || action == "reset")
    {
        string& draft = bcDraft[eventIn.user.id];
        if (action == "num")
        {
            if (eventIn.words.size() != 3 || eventIn.words[2].size() != 1 || !isdigit((unsigned char)eventIn.words[2][0]))
                return;
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
            return;
        }
        if (!draft.size())
        {
            eventOut.msg += "нечего проверять — набери число.";
            eventOut.keyboard = bcKeyboard().dump();
            return;
        }
        eventIn.words[1] = draft; // дальше как обычный ответ цифрой
        draft.clear();
    }

    if (eventIn.words.size() < 2)
    {
        eventOut.msg += "ты не указал четырёхзначное число.";
        eventOut.keyboard = bcKeyboard().dump();
        return;
    }
    string userNumber = eventIn.words[1];
    if (!(userNumber.size() == 4 && str::fromString(userNumber) && userNumber[0] != '0')) // если это не четырёх значное число
    {
        eventOut.msg += "ты указал не четырёхзначное число.";
        eventOut.keyboard = bcKeyboard().dump();
        return;
    }

    if (userNumber == randNumbers[eventIn.user.id])
    {
        eventOut.msg += "молодец!!! Ты отгадал число!";
        randNumbers.erase(eventIn.user.id);
        bcDraft.erase(eventIn.user.id);
        return;
    }

    string randNumber = randNumbers[eventIn.user.id];

    uint8_t c = 0; // коровы
    uint8_t b = 0; // быки
    for (uint8_t ri = 0; ri < 4; ri++)
    {
        if (randNumber[ri] == userNumber[ri]) // если бык
        {
            b++;
            continue;
        }
        for (uint8_t ui = 0; ui < 4; ui++)
            if (randNumber[ri] == userNumber[ui]) // если корова
            {
                c++;
                break;
            }
    }

    eventOut.msg = "найдено:\n  Коров: " + to_string(c) + "\n  Быков: " + to_string(b);
    eventOut.keyboard = bcKeyboard().dump();
}
