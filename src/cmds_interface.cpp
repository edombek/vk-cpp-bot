// Интерфейсные команды: /stat, /rename
#include "cmds.h"
#include "keyboard.h"
#include "str.h"
#include "timer.h"
#include "users.h"
#include <cstdio>

using namespace std;

// читает значение «ключ: значение» из псевдофайла /proc
static string getParamOfPath(const string& path, const string& p)
{
    FILE* f = fopen(path.c_str(), "r");
    if (!f)
        return "";
    char buffer[4096];
    size_t bytes_read = fread(buffer, 1, sizeof(buffer), f);
    fclose(f);
    string dat(buffer, bytes_read);
    args_t lines = str::words(dat, '\n');
    for (auto line : lines)
    {
        args_t param = str::words(line, ':');
        if (param.size() < 2) continue;
        if (str::at(param[0], p))
            return str::replase(param[1], "  ", " ");
    }
    return "";
}

void stat(Event &eventIn, Event &eventOut, Worker *worker)
{
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

    eventOut.msg += "CPU:" + (getParamOfPath("/proc/cpuinfo", "model name").size() ? getParamOfPath("/proc/cpuinfo", "model name") : getParamOfPath("/proc/cpuinfo", "Model")) + "\n";
    eventOut.msg += "RAM: " + usedMem + "/" + allMem + " Мб\n";
    eventOut.msg += "Я сожрал оперативы: " + myMem + " Мб\n";
    eventOut.msg += "Потоков занял: " + getParamOfPath("/proc/self/status", "Threads") + "\n\n";
#endif
    eventOut.msg += "Всего сообщений от тебя: " + std::to_string(eventOut.user.msgs) + "\n";
}

void rename(Event &eventIn, Event &eventOut, Worker *worker)
{
    if (eventIn.words.size() < 2)
        eventOut.msg += "/rename <name>...";
    else
    {
        eventIn.user.name = str::summ(eventIn.words, 1);
        users::changeUser(eventIn.user.id, eventIn.user);
        eventOut.msg += "done!";
    }
}
