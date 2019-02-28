#include "cmd.h"
#include "common.h"
#include "events.h"
#include "fs.h"
#include "img.h"
#include "str.h"

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

#include "../version.h"
#include <chrono>
#include <iostream>
void test(cmdHead)
{
    std::chrono::time_point<std::chrono::system_clock> begin, end;
    begin = std::chrono::system_clock::now();
    eventOut->net->send("http://api.vk.com");
    end = std::chrono::system_clock::now();
    unsigned int t = std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count();
    eventOut->msg += GIT_URL;
    eventOut->msg += " (" + string(FULLVERSION_STRING) + ")\n";
    eventOut->msg += "Обработка VK API за: " + to_string(t) + "мс\n";
    eventOut->msg += "id чата (пользователь/чат): " + to_string(eventIn->from_id) + "/" + to_string(eventIn->peer_id) + "\n";

    //получаем использование памяти
    string allMem = to_string((int)((float)str::fromString(getParamOfPath("/proc/meminfo", "MemTotal")) / 1024));
    string usedMem = to_string((int)((float)(str::fromString(getParamOfPath("/proc/meminfo", "MemTotal")) - str::fromString(getParamOfPath("/proc/meminfo", "MemAvailable"))) / 1024));
    string myMem = to_string((int)((float)str::fromString(getParamOfPath("/proc/self/status", "VmRSS")) / 1024));

    eventOut->msg += "CPU:" + getParamOfPath("/proc/cpuinfo", "model name") + "\n";
    eventOut->msg += "RAM: " + usedMem + "/" + allMem + " Мб\n";
    eventOut->msg += "Я сожрал оперативы: " + myMem + " Мб\n";
    eventOut->msg += "Потоков занял: " + getParamOfPath("/proc/self/status", "Threads") + "\n\n";

    eventOut->msg += "Всего сообщений от тебя: " + std::to_string(eventOut->user.msgs) + "\n";

    for (auto doc : eventIn->docs) {
        img im(doc, eventOut->net);
        eventOut->docs.push_back(im.getDoc("photo", "doc", eventOut->peer_id, eventOut->net, eventOut->vk));
    }
}

void con(cmdHead)
{
    char buffer[512];
#ifdef __linux__
    args_t commands = str::words(str::summ(eventIn->words, 1), '\n');
    string c = "";
    for (auto command : commands)
        c += command + " 2>&1\n";
#elif _WIN32
    string c = str::summ(eventIn->words, 1);
#endif

    FILE* pipe = popen(c.c_str(), "r");
    while (fgets(buffer, sizeof buffer, pipe) != NULL) {
        eventOut->msg = buffer;
        eventOut->send();
    }
    eventOut->msg = "done!";
    pclose(pipe);
}

void upload(cmdHead)
{
    string filename = str::summ(eventIn->words, 1);
    cout << filename << endl;
    string dat = fs::readData(filename);
    eventOut->docs.push_back(new Doc("doc", to_string(eventIn->peer_id), filename, dat, eventIn->net, eventIn->vk));
}
