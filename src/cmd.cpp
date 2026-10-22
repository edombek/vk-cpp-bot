#include "cmd.h"
#include "cmds.h"
#include "keyboard.h"
#include "str.h"
#include <map>
#include <vector>

using namespace std;

typedef struct cmd_t
{
    string info;
    msg_func_t func;
    uint8_t acess;
    bool disp;
    string cat; // раздел для навигации по справке (кнопки)
} cmd_t;
typedef map<string, cmd_t> cmds_t;

//прописываем в системе команд
cmds_t cmdsList =
{
    { "/help", { "вывод команд", &help, 1, true, "интерфейс" } },
    { "/setc", { "редактор конфига", &setCfg, 5, true, "админ" } },
    { "/stat", { "статус бота", &stat, 1, true, "интерфейс" } },
    { "/con", { "консоль)", &con, 5, true, "админ" } },
    { "/u", { "загрузка файла", &upload, 5, true, "админ" } },
    { "/set", { "установка уровня доступа человеку", &setc, 5, true, "админ" } },
    { "/rename", { "установить ник в боте", &rename, 1, true, "интерфейс" } },
    { "/vid", { "поиск видео", &videos, 3, true, "медиа" } },
    { "/w", { "погода", &weather, 1, true, "утилиты" } },
    { "/g", { "игра", &gameF, 1, true, "игры" } },
    { "/apod", { "астрофото на каждый день", &apod, 1, true, "медиа" } },
    {"/py", { "интерпретатор душманского змия", &pycmd, 5, true, "админ" }},
    {"/pix", { "8bit art", &pix, 1, true, "медиа" }},
    {"/vox", { "Half-Life vox", &vox, 1, true, "медиа" }},
    {"/crt", { "cartoon art", &crt, 1, true, "медиа" }},
//    {"/l", { "рисовка линий из изображения", &line, 1, true, "медиа" }},
    {"/bc", { "игра: \"Быки и коровы\"", &bc, 1, true, "игры" }},
    {"/neon", { "неон арт", &neon, 1, true, "медиа" }}
};

// порядок разделов в справке
static const vector<string> cmdCats = { "интерфейс", "утилиты", "игры", "медиа", "админ" };

void help(Event &eventIn, Event &eventOut, Worker *worker)
{
    string section = eventIn.words.size() > 1 ? str::low(eventIn.words[1]) : "";
    if (section == "все" || section == "все команды")
        section = "";
    eventOut.msg += "твой уровень прав: " + to_string(eventIn.user.acess);
    if (section.size())
        eventOut.msg += ", раздел: " + section;
    eventOut.msg += "\n";
    Keyboard kbd(false, false);
    for (const auto& cat : cmdCats)
    {
        // раздела без доступных пользователю команд не показываем
        bool any = false;
        for (const auto& cmdInfo : cmdsList)
            if (cmdInfo.second.disp && cmdInfo.second.cat == cat && cmdInfo.second.acess <= eventIn.user.acess)
                any = true;
        if (!any || (section.size() && section != cat))
            continue;
        if (section.size()) // внутри раздела печатаем команды
            for (const auto& cmdInfo : cmdsList)
            {
                if (!cmdInfo.second.disp || cmdInfo.second.cat != cat || eventIn.user.acess < cmdInfo.second.acess)
                    continue;
                eventOut.msg += cmdInfo.first + " - " + cmdInfo.second.info + " (" + to_string(cmdInfo.second.acess) + ")\n";
            }
        else
            kbd.button(cat, "/help " + cat);
    }
    if (section.size())
        kbd.button("все разделы", "/help", "primary");
    else
        kbd.button("все команды", "/help все", "primary");
    if (!kbd.empty())
    {
        kbd.row();
        eventOut.keyboard = kbd.dump();
    }
}

#include <cstdio>
bool cmd::start(const std::string &comand, Event &eventIn, Event &eventOut, Worker *worker)
{
    if (!findinmap(cmdsList, comand))
        return false;
    const cmd_t &ci = cmdsList[comand];
    if (ci.acess > eventIn.user.acess) // права — по входящему событию (eventOut — лишь его копия-ответ)
        return false;
    fprintf(stderr, "\n%s vk.com/id%d (%d/%d(%s))", comand.c_str(), eventIn.user.id, eventIn.user.id, eventIn.peer_id, eventIn.type.c_str());
    ci.func(eventIn, eventOut, worker);
    fprintf(stderr, "\n%s vk.com/id%d (%d/%d(%s))-done!\n", comand.c_str(), eventIn.user.id, eventIn.user.id, eventIn.peer_id, eventIn.type.c_str());
    return true;
}
