#include "cmd.h"
#include "common.h"
#include "events.h"
#include "fs.h"
#include "img.h"
#include "str.h"
#include <iostream>

using namespace std;

#include <stdio.h>
string getParamOfPath(string path, string p)
{
    FILE* f = fopen(path.c_str(), "r");
    char buffer[4096];
    size_t bytes_read;
    bytes_read = fread(buffer, 1, sizeof(buffer), f);
    string dat(buffer, bytes_read);
    args_t lines = str::words(dat, '\n');
    //cout << lines[0] << endl;
    for (auto line : lines) {
        args_t param = str::words(line, ':');
        if (str::at(param[0], p))
            return str::replase(param[1], "  ", " ");
    }
    return "";
}

#include <chrono>
void stat(cmdHead)
{
    std::chrono::time_point<std::chrono::system_clock> begin, end;
    begin = std::chrono::system_clock::now();
    eventOut.net.send("http://api.vk.com");
    end = std::chrono::system_clock::now();
    uint32_t t = std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count();
#ifndef WIN32
    eventOut.msg += GIT_URL;
    eventOut.msg += " (" + string(GIT_VER) + ")\n";
#endif
    eventOut.msg += "Обработка VK API за: " + to_string(t) + "мс\n";
    eventOut.msg += "id чата (пользователь/чат): " + to_string(eventIn.from_id) + "/" + to_string(eventIn.peer_id) + "\n";
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
#ifdef __linux__
    args_t commands = str::words(str::summ(eventIn.words, 1), '\n');
    string c = "";
    for (auto command : commands)
        c += command + " 2>&1\n";
#elif WIN32
    string c = str::summ(eventIn.words, 1);
#endif

    FILE* pipe = popen(c.c_str(), "r");
    while (fgets(buffer, sizeof buffer, pipe) != NULL) {
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
    else {
        uint32_t id = str::fromString(eventIn.words[1]);
        if (!id)
            eventOut.msg += "/set <id> <level>...";
        else {
            users::user user = users::getUser(id, eventOut.vk);
            user.acess = str::fromString(eventIn.words[2]);
            user.msgs--;
            users::changeUser(id, user);
            eventOut.msg += "done!";
        }
    }
}

void rename(cmdHead)
{
    if (eventIn.words.size() < 2) // != <cmd, id, level>
        eventOut.msg += "/rename <name>...";
    else {
        eventIn.user.name = str::summ(eventIn.words, 1);
        users::changeUser(eventIn.user.id, eventIn.user);
        eventOut.msg += "done!";
    }
}

void videos(cmdHead)
{
    if (eventIn.words.size() < 2)
        eventOut.msg += "/vid <count> <q>";
    else {
        string q = str::summ(eventIn.words, 1);
        int c = str::fromString(eventIn.words[1]);
        if (!c || c > 200)
            c = 200;
        json resp = eventOut.vk.send("video.search", { { "q", q }, { "count", to_string(c) }, { "adult", "0" }, { "hd", "1" }, { "filters", "mp4" } }, true);
        if (resp["response"].is_null() || resp["response"]["items"].size() == 0) {
            eventOut.msg = "nope";
            return;
        }
        string msg = eventOut.msg;
        string vidc = "/" + to_string(resp["response"]["items"].size());
        for (uint16_t i = 0; i < resp["response"]["items"].size(); i++) {
            json r;
            r["video"] = resp["response"]["items"][i];
            r["type"] = "video";
            Doc doc(r);
            eventOut.docs = { doc };
            eventOut.msg = msg + to_string(i + 1) + vidc;
            eventOut.send();
        }
        eventOut.msg = msg + "держи";
        eventOut.docs = {};
    }
}

typedef struct {
    float x;
    float y;
} xy_t;

typedef struct {
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
    if (!eventIn.docs.size()) {
        eventOut.msg += "прикрепи фото";
        return;
    }
    int32_t c = 1;
    if (eventIn.words.size() > 1)
        c = str::fromString(eventIn.words[1]);
    if (c < 1)
        c = 1;

    for (auto doc : eventIn.docs) {
        img im(doc, eventIn.net);
        xy_t o = { im.im->sx / 2.0f, im.im->sy / 2.0f };
        float r;
        if (o.x > o.y)
            r = o.y;
        else
            r = o.x;
        img balled(2 * r, 2 * r);
        xy_t xy;
        ra_t ra;
        xy_t xyO;
        for (uint32_t yc = 0; yc < balled.im->sy; yc++)
            for (uint32_t xc = 0; xc < balled.im->sx; xc++) {
                xy.x = (xc - r) / r;
                xy.y = (yc - r) / r;
                ra = toRA(xy);
                if (ra.r * ra.r > 1) {
                    gdImageSetPixel(balled.im, xc, yc, 0xFFFFFFFF);
                    continue;
                }
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
    if (!eventIn.docs.size()) {
        eventOut.msg += "прикрепи фото";
        return;
    }
    int32_t c = 1;
    if (eventIn.words.size() > 1)
        c = str::fromString(eventIn.words[1]);
    if (c < 1)
        c = 1;
    for (auto doc : eventIn.docs) {
        img im(doc, eventIn.net);
        xy_t o = { im.im->sx / 2.0f, im.im->sy / 2.0f };
        float r;
        if (o.x > o.y)
            r = o.y;
        else
            r = o.x;
        img balled(2 * r, 2 * r);
        xy_t xy;
        ra_t ra;
        xy_t xyO;
        for (uint32_t yc = 0; yc < balled.im->sy; yc++)
            for (uint32_t xc = 0; xc < balled.im->sx; xc++) {
                xy.x = (xc - r) / r;
                xy.y = (yc - r) / r;
                ra = toRA(xy);
                if (ra.r * ra.r > 1) {
                    gdImageSetPixel(balled.im, xc, yc, 0xFFFFFFFF);
                    continue;
                }
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
