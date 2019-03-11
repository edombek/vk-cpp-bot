#include "cmd.h"
#include <map>

using namespace std;

typedef void (*msg_func_t)(Event* eventIn, Event* eventOut); // func(eventIn, eventOut)
typedef struct {
    string info;
    msg_func_t func;
    uint8_t acess;
    bool disp;
} cmd_t;
typedef map<string, cmd_t> cmds_t;

//объявляем команды
void help(cmdHead);
void test(cmdHead);
void con(cmdHead);
void upload(cmdHead);
void set(cmdHead);
void rename(cmdHead);

//прописываем в системе команд
cmds_t cmdsList = {
    { "/help", { "вывод команд", &help, 1, true } },
    { "/stat", { "статус бота", &test, 1, true } },
    { "/con", { "консоль)", &con, 5, true } },
    { "/u", { "загрузка файла", &upload, 5, true } },
    { "/set", { "установка уровня доступа человеку", &set, 5, true } },
    { "/rename", { "установить ник в боте", &rename, 1, true } }
};

void help(cmdHead)
{
    eventOut->msg += "твой уровень прав: " + to_string(eventOut->user.acess) + "\n";
    for (auto cmdInfo : cmdsList) {
        if (!cmdInfo.second.disp && eventOut->user.acess < cmdInfo.second.acess)
            continue;
        eventOut->msg += cmdInfo.first + " - " + cmdInfo.second.info + " (" + to_string(cmdInfo.second.acess) + ")\n";
    }
}

#include <cstdio>
bool cmd::start(std::string comand, Event* eventIn, Event* eventOut)
{
    if (cmdsList.find(comand) == cmdsList.cend())
        return false;
    if (cmdsList[comand].acess > eventOut->user.acess)
        return false;
    fprintf(stderr, "\n%s vk.com/id%d (%d/%d(%s))", comand.c_str(), eventIn->user.id, eventIn->user.id, eventIn->peer_id, eventIn->type.c_str());
    cmdsList[comand].func(eventIn, eventOut);
    fprintf(stderr, "\r%s vk.com/id%d (%d/%d(%s))-done!\n", comand.c_str(), eventIn->user.id, eventIn->user.id, eventIn->peer_id, eventIn->type.c_str());
    return true;
}
