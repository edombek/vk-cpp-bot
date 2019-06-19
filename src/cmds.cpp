#include "cmd.h"
#include "common.h"
#include "events.h"
#include "fs.h"
#include "img.h"
#include "str.h"
#include "timer.h"
#include <iostream>

#define MAX(a,b) ((a) > (b) ? (a) : (b))
#define MIN(a,b) ((a) < (b) ? (a) : (b))

using namespace std;

#include <stdio.h>
string getParamOfPath(string path, string p)
{
    FILE* f = fopen(path.c_str(), "r");
    char buffer[4096];
    size_t bytes_read;
    bytes_read = fread(buffer, 1, sizeof(buffer), f);
    fclose(f);
    string dat(buffer, bytes_read);
    args_t lines = str::words(dat, '\n');
    //cout << lines[0] << endl;
    for (auto line : lines)
    {
        args_t param = str::words(line, ':');
        if (str::at(param[0], p))
            return str::replase(param[1], "  ", " ");
    }
    return "";
}

void stat(cmdHead)
{
    std::chrono::time_point<std::chrono::system_clock> begin, end;
    timer t;
    eventOut.net.send("http://api.vk.com");
#ifndef WIN32
    eventOut.msg += GIT_URL;
    eventOut.msg += " (" + string(GIT_VER) + ")\n";
#endif
    eventOut.msg += "Обработка VK API за: " + to_string(t.getWorked()) + "мс\n";
    eventOut.msg += "id чата (пользователь/чат): " + to_string(eventIn.from_id) + "/" + to_string(eventIn.peer_id) + "\n";
    eventOut.msg += "Работаю: " + timer::getWorkTime() + "\n";
#ifndef WIN32
    //получаем использование памяти
    string allMem = to_string((int)((float)str::fromString(getParamOfPath("/proc/meminfo", "MemTotal")) / 1024));
    string usedMem = to_string((int)((float)(str::fromString(getParamOfPath("/proc/meminfo", "MemTotal")) - str::fromString(getParamOfPath("/proc/meminfo", "MemAvailable"))) / 1024));
    string myMem = to_string((int)((float)str::fromString(getParamOfPath("/proc/self/status", "VmRSS")) / 1024));

    eventOut.msg += "CPU:" + getParamOfPath("/proc/cpuinfo", "model name") + "\n";
    eventOut.msg += "RAM: " + usedMem + "/" + allMem + " Мб\n";
    eventOut.msg += "Я сожрал оперативы: " + myMem + " Мб\n";
    eventOut.msg += "Потоков занял: " + getParamOfPath("/proc/self/status", "Threads") + "\n\n";
#endif
    eventOut.msg += "Всего сообщений от тебя: " + std::to_string(eventOut.user.msgs) + "\n";
}

#ifdef WIN32 //винда чо ¯\_(ツ)_/¯
#define popen _popen
#define pclose _pclose
#endif
void con(cmdHead)
{
    char buffer[512];

    args_t commands = str::words(str::summ(eventIn.words, 1), '\n');
    string c = "";
    for (auto command : commands)
        c += command + " 2>&1\n";

    FILE* pipe = popen(c.c_str(), "r");
    while (fgets(buffer, sizeof buffer, pipe) != NULL)
    {
        eventOut.msg = buffer;
        eventOut.send();
    }
    eventOut.msg = "done!";
    pclose(pipe);
}

void upload(cmdHead)
{
    string filename = str::summ(eventIn.words, 1);
    string dat = fs::readData(filename);
    Doc doc;
    if (doc.uploadDoc(filename, dat, eventOut.net, eventOut.vk, eventOut.peer_id))
        eventOut.docs.push_back(doc);
}

void set(cmdHead)
{
    if (eventIn.words.size() != 3) // != <cmd, id, level>
        eventOut.msg += "/set <id> <level>...";
    else
    {
        uint32_t id = str::fromString(eventIn.words[1]);
        if (!id)
            eventOut.msg += "/set <id> <level>...";
        else
        {
            users::user user = users::getUser(id, eventOut.vk);
            user.acess = str::fromString(eventIn.words[2]);
            user.msgs--;
            users::changeUser(id, user);
            eventOut.msg += "done!";
        }
    }
}

void stop(cmdHead)
{
    eventOut.msg += "done!\nwaiting lp...";
    worker->stop();
}

void rename(cmdHead)
{
    if (eventIn.words.size() < 2) // != <cmd, id, level>
        eventOut.msg += "/rename <name>...";
    else
    {
        eventIn.user.name = str::summ(eventIn.words, 1);
        users::changeUser(eventIn.user.id, eventIn.user);
        eventOut.msg += "done!";
    }
}

void videos(cmdHead)
{
    if (eventIn.words.size() < 2)
        eventOut.msg += "/vid <count> <q>";
    else
    {
        string q;
        int c = str::fromString(eventIn.words[1]);
        if (c)
            q = str::summ(eventIn.words, 2);
        else
            q = str::summ(eventIn.words, 1);
        if (!c || c > 200)
            c = 200;
        json resp = eventOut.vk.send("video.search", { { "q", q }, { "count", to_string(c) }, { "adult", "1" }, { "hd", "0" }, { "filters", "mp4" } }, true);
        if (resp["response"].is_null() || resp["response"]["items"].size() == 0)
        {
            eventOut.msg = "nope";
            return;
        }
        string msg = eventOut.msg;
        eventOut.msg = msg + "держи";
        string vidc = "/" + to_string(resp["response"]["items"].size());
        uint8_t s = 5;
        if (eventOut.type.find("wall_post_") != eventOut.type.npos)
            s = 2;
        for (uint16_t i = 0; i < resp["response"]["items"].size(); i++)
        {
            json r;
            r["video"] = resp["response"]["items"][i];
            r["type"] = "video";
            eventOut.docs.emplace_back(r);
            if(eventOut.docs.size() == s)
            {
                eventOut.send();
                eventOut.docs = {};
            }
        }
        eventOut.msg = msg + "всё)";
    }
}

typedef struct
{
    float x;
    float y;
} xy_t;

typedef struct
{
    float r;
    float a;
} ra_t;

#include <cmath>
ra_t toRA(xy_t c)
{
    ra_t ra;
    ra.a = atan(c.y / c.x);
    if (c.x >= 0)
        ra.r = sqrt(c.x * c.x + c.y * c.y);
    else
        ra.r = -sqrt(c.x * c.x + c.y * c.y);
    return ra;
}

xy_t toXY(ra_t c)
{
    xy_t xy;
    xy.x = c.r * cos(c.a);
    xy.y = c.r * sin(c.a);
    return xy;
}

void asin(cmdHead)
{
    if (!eventIn.docs.size())
    {
        eventOut.msg += "прикрепи фото";
        return;
    }
    int32_t c = 1;
    if (eventIn.words.size() > 1)
        c = str::fromString(eventIn.words[1]);
    if (c < 1)
        c = 1;

    for (auto doc : eventIn.docs)
    {
        img im(doc, eventIn.net);
        xy_t o = { im.im->sx / 2.0f, im.im->sy / 2.0f };
        float r = sqrt(im.im->sx * im.im->sx + im.im->sy * im.im->sy) / 2;
        xy_t s = {im.im->sx / 1.0f, im.im->sy / 1.0f};
        for (int32_t i = 0; i < c; i++)
            s = {(float)(asin(s.x/r/2)*r*2 / M_PI * 2.0f), (float)(asin(s.y/r/2)*r*2 / M_PI * 2.0f)};
        float rB = sqrt(s.x*s.x + s.y*s.y) / 2;
        img balled(s.x, s.y);
        xy_t xy;
        ra_t ra;
        xy_t xyO;
        xy_t oB = { balled.im->sx / 2.0f, balled.im->sy / 2.0f };
        for (uint32_t yc = 0; yc < balled.im->sy; yc++)
            for (uint32_t xc = 0; xc < balled.im->sx; xc++)
            {
                xy.x = (xc - oB.x) / rB;
                xy.y = (yc - oB.y) / rB;
                ra = toRA(xy);
                for (int32_t i = 0; i < c; i++)
                    ra.r = asin(ra.r) / M_PI * 2;
                xyO = toXY(ra);
                gdImageSetPixel(balled.im, xc, yc, gdImageGetPixel(im.im, xyO.x * r + o.x, xyO.y * r + o.y));
            }
        eventOut.docs.push_back(balled.getPhoto(eventIn.peer_id, eventIn.net, eventIn.vk));
        eventOut.docs.push_back(balled.getDoc(eventIn.peer_id, eventIn.net, eventIn.vk));
        eventOut.send();
        eventOut.docs = {};
    }
    eventOut.msg += "готово)";
}

void sin(cmdHead)
{
    if (!eventIn.docs.size())
    {
        eventOut.msg += "прикрепи фото";
        return;
    }
    int32_t c = 1;
    if (eventIn.words.size() > 1)
        c = str::fromString(eventIn.words[1]);
    if (c < 1)
        c = 1;
    for (auto doc : eventIn.docs)
    {
        img im(doc, eventIn.net);
        xy_t o = { im.im->sx / 2.0f, im.im->sy / 2.0f };
        float r = sqrt(im.im->sx * im.im->sx + im.im->sy * im.im->sy) / 2;
        img balled(im.im->sx, im.im->sy);
        xy_t xy;
        ra_t ra;
        xy_t xyO;
        for (uint32_t yc = 0; yc < balled.im->sy; yc++)
            for (uint32_t xc = 0; xc < balled.im->sx; xc++)
            {
                xy.x = (xc - o.x) / r;
                xy.y = (yc - o.y) / r;
                ra = toRA(xy);
                for (int32_t i = 0; i < c; i++)
                    ra.r = sin(ra.r * M_PI / 2);
                xyO = toXY(ra);
                gdImageSetPixel(balled.im, xc, yc, gdImageGetPixel(im.im, xyO.x * r + o.x, xyO.y * r + o.y));
            }
        eventOut.docs.push_back(balled.getPhoto(eventIn.peer_id, eventIn.net, eventIn.vk));
        eventOut.docs.push_back(balled.getDoc(eventIn.peer_id, eventIn.net, eventIn.vk));
        eventOut.send();
        eventOut.docs = {};
    }
    eventOut.msg += "готово)";
}

#define openweathermap_msg "get token on openweathermap.org"
void weather(cmdHead)
{
    if (eventIn.words.size() < 2)
    {
        eventOut.msg += "мб город введёшь?";
        return;
    }

    //вытаскиваем токен от сервиса
    conf.lock.lock();
    json& c = conf.get();
    if (c["openweathermap_token"].is_null() || c["openweathermap_token"] == openweathermap_msg)
    {
        c["openweathermap_token"] = openweathermap_msg;
        conf.lock.unlock();
        conf.save();
        eventOut.msg += "передайте админу что он лентяй)";
        return;
    }
    string appid(c["openweathermap_token"].get<string>());
    conf.lock.unlock();

    table_t params =
    {
        { "lang", "ru" },
        { "units", "metric" },
        { "APPID", appid },
        { "q", str::summ(eventIn.words, 1) }
    };
    json weather = json::parse(eventIn.net.send("http://api.openweathermap.org/data/2.5/weather", params, false));
    if (weather["main"].is_null())
    {
        eventOut.msg += "чтота пошло не так, возможно надо ввести город транслитом или требуется вмешательство внешних сил(админа)";
        return;
    }
    string temp = "";
    temp += "погода в " + weather["sys"]["country"].get<string>() + "/" + weather["name"].get<string>() + ":";
    //temp += "\n¤" + other::getTime(weather["dt"]) + ":\n";
    temp += "\n•температура: " + to_string((int)weather["main"]["temp"]) + "°C\n•скорость ветра: " + to_string((int)weather["wind"]["speed"]) + "м/с\n•влажность: " + to_string((int)weather["main"]["humidity"]) + "%\n•описание: " + weather["weather"][0]["description"].get<string>() + "\n";
    eventOut.msg += temp;
}

void setCfg(cmdHead)
{
    if (eventIn.words.size() < 2 || eventIn.words.size() > 3)
    {
        eventOut.msg += "<param> <value> or <param>";
        return;
    }
    string param = eventIn.words[1];
    conf.lock.lock();
    if (eventIn.words.size() == 2)
    {
        if (!conf.get()[param].is_null())
            eventOut.msg += conf.get()[param].dump();
        else
            eventOut.msg += "NULL";
        conf.lock.unlock();
        return;
    }
    conf.get()[param] = json::parse(eventIn.words[2]);
    conf.lock.unlock();
    conf.save();
}

#include "game.h"
map<int, game*> gameNew = {}; // новые игры в чате
map<int, map<int, game*>> gameUsers = {}; // все игры в [chat][user]
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

void gameF(cmdHead)
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
