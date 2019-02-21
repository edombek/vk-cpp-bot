#ifndef CMDS_HPP_INCLUDED
#define CMDS_HPP_INCLUDED
#include "common.h"
#include "events.h"
#include "fs.h"
#include "str.h"

#define cmdHead Event *eventIn, Event *eventOut

using namespace std;

string getParamOfPath(string path, string p)
{
    string dat = fs::readData(path);
    args_t lines = str::words(dat, '\n');
    for (auto line : lines) {
        args_t param = str::words(line, ':');
        if (str::at(param[0], p))
            return str::replase(param[1], "  ", " ");
    }
    return "";
}

#include <chrono>
void test(cmdHead)
{
    std::chrono::time_point<std::chrono::system_clock> begin, end;
    begin = std::chrono::system_clock::now();
    eventIn->net->send("http://api.vk.com", "");
    end = std::chrono::system_clock::now();
    unsigned int t = std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count();
    eventOut->msg += "Обработка VK API за: " + to_string(t) + "мс\n";
    eventOut->msg += "id чата (пользователь/чат): " + to_string(eventIn->from_id) + "/" + to_string(eventIn->peer_id) + "\n";

    //получаем использование памяти
    string allMem = to_string((int)((float)str::fromString(getParamOfPath("/proc/meminfo", "MemTotal")) / 1024));
    string usedMem = to_string((int)((float)(str::fromString(getParamOfPath("/proc/meminfo", "MemTotal")) - str::fromString(getParamOfPath("/proc/meminfo", "MemAvailable"))) / 1024));
    string myMem = to_string((int)((float)str::fromString(getParamOfPath("/proc/self/status", "VmRSS")) / 1024));

    eventOut->msg += "CPU:" + getParamOfPath("/proc/cpuinfo", "model name") + "\n";
    eventOut->msg += "Потоков занято: " + getParamOfPath("/proc/self/status", "Threads") + "\n";
    eventOut->msg += "Я сожрал оперативы: " + myMem + " Мб\n";
}

#endif // CMDS_HPP_INCLUDED
