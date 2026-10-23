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
    // ярлыки разделов справки: что показывают кнопки, то можно ввести текстом (/игры и т.п.)
    { "/интерфейс", { "раздел справки", &help, 1, false, "интерфейс" } },
    { "/утилиты", { "раздел справки", &help, 1, false, "утилиты" } },
    { "/игры", { "раздел справки", &help, 1, false, "игры" } },
    { "/медиа", { "раздел справки", &help, 1, false, "медиа" } },
    { "/админ", { "раздел справки", &help, 1, false, "админ" } },
    { "/все", { "полный список команд", &help, 1, false, "интерфейс" } },
    { "/всекоманды", { "полный список команд", &help, 1, false, "интерфейс" } },
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
    {"/pywiki", { "справка по python-скриптам", &pywiki, 1, true, "интерфейс" }},
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
    bool allCmds = (section == "все" || section == "все команды" || section == "всекоманды");
    string w0 = eventIn.words.size() ? str::low(eventIn.words[0]) : "";
    if (w0 == "/все" || w0 == "/всекоманды") // ярлык полного списка текстом (и «/все команды» с пробелом)
        allCmds = true;
    else if (section.empty() && !w0.empty())
        for (const string& cat : cmdCats) // ярлык раздела текстом: /игры == /help игры
            if (w0 == "/" + cat)
                section = cat;
    if (allCmds)
        section = "";
    eventOut.msg += "твой уровень прав: " + to_string(eventIn.user.acess);
    if (section.size())
        eventOut.msg += ", раздел: " + section;
    else if (allCmds)
        eventOut.msg += ", все команды";
    eventOut.msg += "\n";
    Keyboard kbd(false, false);
    string catsLine; // «интерфейс (5), утилиты (2)...» — подгруппы команд с количеством
    string textHint; // « /интерфейс /утилиты...» — текстовые аналоги кнопок для доступных разделов
    for (const auto& cat : cmdCats)
    {
        // раздела без доступных пользователю команд не показываем
        int cnt = 0;
        for (const auto& cmdInfo : cmdsList)
            if (cmdInfo.second.disp && cmdInfo.second.cat == cat && cmdInfo.second.acess <= eventIn.user.acess)
                cnt++;
        if (!cnt)
            continue;
        textHint += " /" + cat;
        if (section.size() && section != cat)
            continue;
        if (!section.size()) // верхний уровень и полный список — кнопка раздела
            kbd.button(cat, "/help " + cat);
        if (section.size() || allCmds) // внутри раздела (или полный список) печатаем команды
        {
            if (allCmds)
                eventOut.msg += "\n[ " + cat + " ]\n";
            for (const auto& cmdInfo : cmdsList)
            {
                if (!cmdInfo.second.disp || cmdInfo.second.cat != cat || eventIn.user.acess < cmdInfo.second.acess)
                    continue;
                eventOut.msg += cmdInfo.first + " - " + cmdInfo.second.info + " (" + to_string(cmdInfo.second.acess) + ")\n";
            }
        }
        else
            catsLine += (catsLine.empty() ? "" : ", ") + cat + " (" + to_string(cnt) + ")";
    }
    if (section.size())
    {
        kbd.button("все разделы", "/help", "primary").button("все команды", "/help все", "primary");
        eventOut.msg += "\nэто не все команды — они поделены на разделы; кнопки ниже работают и текстом:" + textHint + ", полный список — /всекоманды\n";
    }
    else if (allCmds)
        eventOut.msg += "\nэто полный список; по разделам (подгруппам) короче — кнопки ниже или текстом:" + textHint + "\n";
    else
    {
        eventOut.msg += "команды поделены на разделы (подгруппы): " + catsLine + "\n";
        eventOut.msg += "раздел открывается кнопкой или текстом:" + textHint + "; полный список — «все команды» или /всекоманды\n";
    }
    if (!kbd.empty())
        eventOut.keyboard = kbd.dump();
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
