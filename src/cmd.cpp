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

void help(cmdHead);
void test(cmdHead);

cmds_t cmdsList = {
    { "/help", { "вывод команд", &help, 1, true } },
    { "/stat", { "статус бота", &test, 1, true } }
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

bool cmd::start(std::string comand, Event* eventIn, Event* eventOut)
{
    if (cmdsList.find(comand) == cmdsList.cend())
        return false;
    if (cmdsList[comand].acess > eventOut->user.acess)
        return false;
    //eventOut->msg="true";
    cmdsList[comand].func(eventIn, eventOut);
    return true;
}
