#include "cmd.h"
#include "cmds.h"
#include "bd.h"
#include "keyboard.h"
#include "str.h"
#include <algorithm>
#include <iostream>
#include <map>
#include <mutex>
#include <vector>

#ifndef NO_PYTHON
#include "fs.h"
#include "py.h"
#include <filesystem>
#endif // NO_PYTHON

using namespace std;

typedef struct cmd_t
{
    string info;
    msg_func_t func;
    uint8_t acess;
    bool disp;
    string cat;  // раздел для навигации по справке (кнопки)
    bool pyAddon = false; // команда зарегистрирована из py_addons/ (фаза 12)
    bool pyShortcut = false; // ярлык раздела («/python»), порождён аддонами
} cmd_t;
typedef map<string, cmd_t> cmds_t;

// таблица команд меняется на ходу (cmd::loadAddons / /addonreload из worker-потока) —
// доступ к ней под мьютексом
static std::mutex cmdsMutex;

//прописываем в системе команд
cmds_t cmdsList =
{
    { "/help", { "вывод команд", &help, 1, true, "интерфейс" } },
    // ярлыки разделов справки: что показывают кнопки, то можно ввести текстом (/игры и т.п.)
    { "/интерфейс", { "раздел справки", &help, 1, false, "интерфейс" } },
    { "/утилиты", { "раздел справки", &help, 1, false, "утилиты" } },
    { "/игры", { "раздел справки", &help, 1, false, "игры" } },
    { "/медиа", { "раздел справки", &help, 1, false, "медиа" } },
    { "/космос", { "раздел справки", &help, 1, false, "космос" } },
    { "/админ", { "раздел справки", &help, 1, false, "админ" } },
    { "/все", { "полный список команд", &help, 1, false, "интерфейс" } },
    { "/всекоманды", { "полный список команд", &help, 1, false, "интерфейс" } },
    { "/setc", { "редактор конфига", &setCfg, 5, true, "админ" } },
    { "/stat", { "статус бота", &stat, 1, true, "интерфейс" } },
    { "/con", { "консоль)", &con, 5, true, "админ" } },
    { "/u", { "загрузка файла", &upload, 5, true, "админ" } },
    { "/set", { "установка уровня доступа человеку", &setc, 5, true, "админ" } },
    { "/rename", { "установить ник в боте", &renameUser, 1, true, "интерфейс" } },
    { "/vid", { "поиск видео", &videos, 3, true, "медиа" } },
    { "/w", { "погода", &weather, 1, true, "утилиты" } },
    { "/g", { "игра", &gameF, 1, true, "игры" } },
    { "/apod", { "астрофото на каждый день", &apod, 1, true, "космос" } },
    { "/apoddate", { "астрофото за дату или день рождения", &apoddate, 1, true, "космос" } },
    { "/apodrand", { "астрофото за случайный день", &apodrand, 1, true, "космос" } },
    {"/py", { "интерпретатор душманского змия", &pycmd, 5, true, "админ" }},
    {"/pywiki", { "справка по python-скриптам", &pywiki, 1, true, "интерфейс" }},
    {"/pix", { "8bit art", &pix, 1, true, "медиа" }},
    {"/vox", { "Half-Life vox", &vox, 1, true, "медиа" }},
    {"/crt", { "cartoon art", &crt, 1, true, "медиа" }},
//    {"/l", { "рисовка линий из изображения", &line, 1, true, "медиа" }},
    {"/bc", { "игра: \"Быки и коровы\"", &bc, 1, true, "игры" }},
    {"/neon", { "неон арт", &neon, 1, true, "медиа" }},
    {"/addons", { "список python-команд (py_addons/)", &addons, 1, true, "python" }},
    {"/addonreload", { "перечитать каталог py_addons", &pyreload, 5, true, "админ" }}
};

// порядок встроенных разделов в справке; разделы python-команд добавляются после них
static const vector<string> builtinCats = { "интерфейс", "утилиты", "игры", "медиа", "космос", "админ" };

// разделы с учётом зарегистрированных python-команд (новые — по алфавиту)
static vector<string> catsOf()
{
    vector<string> cats = builtinCats;
    for (const auto& cmdInfo : cmdsList)
    {
        const string& cat = cmdInfo.second.cat;
        if (cmdInfo.second.disp && std::find(cats.begin(), cats.end(), cat) == cats.end())
            cats.push_back(cat); //_cmdsList отсортирован по имени — новые разделы уже по алфавиту
    }
    return cats;
}

void help(Event &eventIn, Event &eventOut, Worker *worker)
{
    std::lock_guard<std::mutex> lk(cmdsMutex); // таблица может перечитываться /addonreload
    const vector<string> cmdCats = catsOf();
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
                eventOut.msg += cmdInfo.first + " - " + cmdInfo.second.info;
                if (cmdInfo.second.acess > 1) // (ур. 1) не пишем — доступно всем; расшифровка в сноске
                    eventOut.msg += " (ур. " + to_string(cmdInfo.second.acess) + ")";
                eventOut.msg += "\n";
            }
        }
        else
            catsLine += (catsLine.empty() ? "" : "\n") + cat + " — " + to_string(cnt); // в столбик: по строке на раздел
    }
    if (section.size())
    {
        kbd.button("все разделы", "/help", "primary").button("все команды", "/help все", "primary");
        eventOut.msg += "\nэто не все команды — они поделены на разделы; кнопки ниже работают и текстом:" + textHint + ", полный список — /всекоманды\n";
    }
    else if (allCmds)
        eventOut.msg += "\n«(ур. N)» — минимальный уровень прав: 1 — любой, 2 — друг, 3 — продвинутый, 5 — админ (у вас " + to_string(eventIn.user.acess) + ");\nэто полный список; по разделам (подгруппам) короче — кнопки ниже или текстом:" + textHint + "\n";
    else
    {
        eventOut.msg += "команды поделены на разделы (подгруппы), после тире — число команд:\n" + catsLine + "\n";
        eventOut.msg += "раздел открывается кнопкой или текстом:" + textHint + "; полный список — «все команды» или /всекоманды\n";
    }
    if (!kbd.empty())
        eventOut.keyboard = kbd.dump();
}

#include <cstdio>
bool cmd::start(const std::string &comand, Event &eventIn, Event &eventOut, Worker *worker)
{
    cmd_t ci; // копируем под локом — /addonreload из другого потока не оставит висячую ссылку
    {
        std::lock_guard<std::mutex> lk(cmdsMutex);
        if (!findinmap(cmdsList, comand))
            return false;
        ci = cmdsList[comand];
    }
    if (ci.acess > eventIn.user.acess) // права — по входящему событию (eventOut — лишь его копия-ответ)
        return false;
    fprintf(stderr, "\n%s vk.com/id%lld (%lld/%lld(%s))", comand.c_str(), (long long)eventIn.user.id, (long long)eventIn.from_id, (long long)eventIn.peer_id, eventIn.type.c_str());
    ci.func(eventIn, eventOut, worker);
    fprintf(stderr, "\n%s vk.com/id%lld (%lld/%lld(%s))-done!\n", comand.c_str(), (long long)eventIn.user.id, (long long)eventIn.from_id, (long long)eventIn.peer_id, eventIn.type.c_str());
    return true;
}

// =========================================================================
// python-аддоны (фаза 12): команды из каталога *.py
// контракт файла: cmd = "/имя"; info = "описание"; acess = 1..5; cat = "раздел";
//                 def run(eventIn, eventOut, net, vk)
// каждый вызов — свежий под-интерпретатор (как /py): глобалы между вызовами
// не сохраняются; код верхнего уровня файла выполняется и при загрузке
// (ради метаданных) — side-эффекты размещать только внутри run()
// =========================================================================
void cmd::loadAddons(const std::string &dir)
{
#ifdef NO_PYTHON
    (void)dir;
#else
    std::string path = dir;
    if (path.empty())
    {
        conf.lock.lock();
        json &c = conf.get();
        if (c["py_addons_dir"].is_string())
            path = c["py_addons_dir"].get<std::string>();
        conf.lock.unlock();
        if (path.empty())
            path = "py_addons";
    }
    std::error_code ec;
    std::vector<std::string> files;
    for (const auto& e : std::filesystem::directory_iterator(path, ec))
        if (!e.is_directory() && e.path().extension() == ".py")
            files.push_back(e.path().string());
    if (ec && files.empty())
    {
        std::cerr << "py_addons: каталог «" << path << "» недоступен (" << ec.message() << "), пропускаю" << std::endl;
        return;
    }
    std::sort(files.begin(), files.end());
    std::lock_guard<std::mutex> lk(cmdsMutex);
    // сначала снимаем ранее зарегистрированные аддоны и их ярлыки (повторный вызов — /addonreload)
    for (auto it = cmdsList.begin(); it != cmdsList.end();)
        if (it->second.pyAddon || it->second.pyShortcut)
            it = cmdsList.erase(it);
        else
            ++it;
    size_t ok = 0;
    vector<string> newCats;
    for (const std::string& file : files)
    {
        std::string source = fs::readData(file);
        PyAddonMeta m;
        try
        {
            m = pyAddonMeta(source);
        }
        catch (const std::exception& e) // создание под-интерпретатора может бросить — не роняем старт
        {
            std::cerr << "py_addons: пропускаю " << file << ": сбой интерпретатора: " << e.what() << std::endl;
            continue;
        }
        if (!m.ok())
        {
            std::cerr << "py_addons: пропускаю " << file << ": " << m.error << std::endl;
            continue;
        }
        m.cmd = str::low(m.cmd); // диспетчер ищет str::low(words[0]) — ключи должны быть в нижнем регистре
        m.cat = str::low(m.cat);
        if (findinmap(cmdsList, m.cmd))
        {
            std::cerr << "py_addons: пропускаю " << file << ": имя " << m.cmd << " уже занято" << std::endl;
            continue;
        }
        std::string src = source;
        cmdsList[m.cmd] = cmd_t{m.info,
            [src](Event &eventIn, Event &eventOut, Worker *worker)
            {
                pyF interpreter(eventIn, eventOut); // под-интерпретатор на вызов (схема /py)
                std::string err = interpreter.execAddon(src);
                if (!err.empty())
                    eventOut.msg += "\n" + err; // traceback скрипта — в ответ, бот не падает
            },
            static_cast<uint8_t>(m.acess), true, m.cat, true};
        if (std::find(builtinCats.begin(), builtinCats.end(), m.cat) == builtinCats.end() &&
            std::find(newCats.begin(), newCats.end(), m.cat) == newCats.end())
            newCats.push_back(m.cat);
        ok++;
    }
    // текстовые ярлыки новых разделов («/python» == /help python) — как у встроенных категорий
    for (const string& cat : newCats)
    {
        string key = "/" + cat;
        if (!findinmap(cmdsList, key))
            cmdsList[key] = cmd_t{ "раздел справки", &help, 1, false, cat, false, true };
    }
    std::cerr << "py_addons: зарегистрировано python-команд: " << ok << " из " << files.size() << std::endl;
#endif // NO_PYTHON
}

std::vector<std::string> cmd::addonList()
{
    std::lock_guard<std::mutex> lk(cmdsMutex);
    std::vector<std::string> res;
    for (const auto& cmdInfo : cmdsList)
        if (cmdInfo.second.pyAddon)
            res.push_back(cmdInfo.first + " — " + cmdInfo.second.info + " (ур. " + std::to_string((int)cmdInfo.second.acess) + ", " + cmdInfo.second.cat + ")");
    return res;
}

