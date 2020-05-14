#include "cmd.h"
#include <map>

using namespace std;

typedef void (*msg_func_t)(cmdHead); // func(cmdHead)
typedef struct
{
    string info;
    msg_func_t func;
    uint8_t acess;
    bool disp;
} cmd_t;
typedef map<string, cmd_t> cmds_t;

//объявляем команды
void help(cmdHead);
void setCfg(cmdHead);
void stat(cmdHead);
void con(cmdHead);
void upload(cmdHead);
void set(cmdHead);
void rename(cmdHead);
void videos(cmdHead);
void asin(cmdHead);
void sin(cmdHead);
void weather(cmdHead);
void gameF(cmdHead);
void apod(cmdHead);
void pycmd(cmdHead);
void pix(cmdHead);
void vox(cmdHead);
void crt(cmdHead);
void line(cmdHead);
void bc(cmdHead);
void neon(cmdHead);

//прописываем в системе команд
cmds_t cmdsList =
{
    { "/help", { "вывод команд", &help, 1, true } },
    { "/setc", { "редактор конфига", &setCfg, 5, true } },
    { "/stat", { "статус бота", &stat, 1, true } },
    { "/con", { "консоль)", &con, 5, true } },
    { "/u", { "загрузка файла", &upload, 5, true } },
    { "/set", { "установка уровня доступа человеку", &set, 5, true } },
    { "/rename", { "установить ник в боте", &rename, 1, true } },
    { "/vid", { "поиск видео", &videos, 3, true } },
    { "/sin", { "изменить преспективу", &sin, 1, true } },
    { "/asin", { "изменить преспективу", &asin, 1, true } },
    { "/w", { "погода", &weather, 1, true } },
    { "/g", { "игра", &gameF, 1, true } },
    { "/apod", { "астрофото на каждый день", &apod, 1, true } },
    {"/py", { "интерпретатор душманского змия", &pycmd, 5, true }},
    {"/pix", { "8bit art", &pix, 1, true }},
    {"/vox", { "Half-Life vox", &vox, 1, true }},
    {"/crt", { "cartoon art", &crt, 1, true }},
    {"/l", { "рисовка линий из изображения", &line, 1, true }},
    {"/bc", { "игра: \"Быки и коровы\"", &bc, 1, true }},
    {"/neon", { "неон арт", &neon, 1, true }}
};

void help(cmdHead)
{
    eventOut.msg += "твой уровень прав: " + to_string(eventOut.user.acess) + "\n";
    for (auto cmdInfo : cmdsList)
    {
        if (!cmdInfo.second.disp || eventOut.user.acess < cmdInfo.second.acess)
            continue;
        eventOut.msg += cmdInfo.first + " - " + cmdInfo.second.info + " (" + to_string(cmdInfo.second.acess) + ")\n";
    }
}

#include <cstdio>
bool cmd::start(std::string comand, cmdHead)
{
    if (!findinmap(cmdsList, comand))
        return false;
    if (cmdsList[comand].acess > eventOut.user.acess)
        return false;
    fprintf(stderr, "\n%s vk.com/id%d (%d/%d(%s))", comand.c_str(), eventIn.user.id, eventIn.user.id, eventIn.peer_id, eventIn.type.c_str());
    cmdsList[comand].func(eventIn, eventOut, worker);
    fprintf(stderr, "\n%s vk.com/id%d (%d/%d(%s))-done!\n", comand.c_str(), eventIn.user.id, eventIn.user.id, eventIn.peer_id, eventIn.type.c_str());
    return true;
}
