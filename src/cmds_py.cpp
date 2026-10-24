// Команды python-интерпретатора: /py (запуск скрипта), /pywiki (справка по /py),
// /addons (список python-команд из py_addons/) и /addonreload (перечитать каталог).
// Вынесены из cmds_admin.cpp: здесь только диспетчерские обёртки над pyF,
// сам интерпретатор и текст вики живут в py.cpp.
#include "cmds.h"
#include "cmd.h"
#include "str.h"

#ifndef NO_PYTHON
#include "py.h"
#endif // NO_PYTHON

using namespace std;

void pycmd(Event &eventIn, Event &eventOut, Worker *worker)
{
#ifndef NO_PYTHON
    std::string str(str::summ(eventIn.words, 1));
    pyF interpreter(eventIn, eventOut); // под-интерпретатор с собственным GIL (pybind11)
    str = interpreter.exec(str);
    if(str.empty())
    {
        if (interpreter.printsSent() == 0)
            eventOut.msg += "\ndone!";
        else if (eventOut.msg.empty())
            eventOut.msg = "готово!"; // print-сообщения уже отправлены, но пустой ответ VK не примет
    }
    else
        eventOut.msg += "\n" + str;
#else
    eventOut.msg = "не собрано";
#endif
}

void pywiki(Event &eventIn, Event &eventOut, Worker *worker)
{
#ifndef NO_PYTHON
    // справка по /py реализуется самим /py: разделы уходят print-сообщениями
    std::string code(pyWikiScript());
    pyF interpreter(eventIn, eventOut);
    std::string err = interpreter.exec(code);
    if (!err.empty())
        eventOut.msg += "\n" + err;
    else if (eventOut.msg.empty())
        eventOut.msg = "вики отправлена сообщениями выше";
#else
    eventOut.msg = "не собрано";
#endif
}

void addons(Event &eventIn, Event &eventOut, Worker *worker)
{
#ifndef NO_PYTHON
    std::vector<std::string> list = cmd::addonList();
    if (list.empty())
        eventOut.msg += "python-команды не загружены — положи .py-файлы в каталог py_addons/ "
                        "(cmd/info/acess/cat + def run(eventIn, eventOut, net, vk)), затем /addonreload";
    else
    {
        eventOut.msg += "python-команды (" + std::to_string(list.size()) + ", каталог py_addons/):\n";
        for (const std::string& s : list)
            eventOut.msg += s + "\n";
    }
#else
    eventOut.msg = "python не собран";
#endif
}

void pyreload(Event &eventIn, Event &eventOut, Worker *worker)
{
#ifndef NO_PYTHON
    cmd::loadAddons();
    eventOut.msg += "каталог py_addons/ перечитан, python-команд: " + std::to_string(cmd::addonList().size())
                    + " (подробности — в логе бота)";
#else
    eventOut.msg = "python не собран";
#endif
}
