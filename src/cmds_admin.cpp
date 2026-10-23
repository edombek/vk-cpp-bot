// Административные команды: /setc, /con, /u, /set, /py
#include "cmds.h"
#include "bd.h"
#include "fs.h"
#include "keyboard.h"
#include "str.h"
#include "timer.h"
#include "users.h"
#include <cstdio>

#ifndef NO_PYTHON
#include "py.h"
#endif // NO_PYTHON

using namespace std;

#ifdef WIN32 //винда чо ¯\_(ツ)_/¯
#define popen _popen
#define pclose _pclose
#endif
void con(Event &eventIn, Event &eventOut, Worker *worker)
{
    char buffer[512];

    args_t commands = str::words(str::summ(eventIn.words, 1), '\n');
    string c = "";
    for (auto command : commands)
        c += command + " 2>&1\n";

    FILE* pipe = popen(c.c_str(), "r");
    if (!pipe)
    {
        eventOut.msg = "не удалось запустить команду";
        return;
    }
    while (fgets(buffer, sizeof buffer, pipe) != NULL)
    {
        eventOut.msg = buffer;
        eventOut.send();
        timer::sleep(1000);
    }
    eventOut.msg = "done!";
    pclose(pipe);
}

void upload(Event &eventIn, Event &eventOut, Worker *worker)
{
    string filename = str::summ(eventIn.words, 1);
    string dat = fs::readData(filename);
    Doc doc;
    if (doc.uploadDoc(filename, dat, eventOut.net, eventOut.vk, eventOut.peer_id))
        eventOut.docs.push_back(doc);
}

void setc(Event &eventIn, Event &eventOut, Worker *worker)
{
    if (eventIn.words.size() != 3) // != <cmd, id, level>
    {
        eventOut.msg += "/set <id> <level>...\nбыстро выставить уровень себе:";
        Keyboard kbd(true, false);
        kbd.button("1 — юзер", "/set " + to_string(eventIn.user.id) + " 1")
            .button("2 — друг", "/set " + to_string(eventIn.user.id) + " 2");
        kbd.row();
        kbd.button("3 — продвинутый", "/set " + to_string(eventIn.user.id) + " 3")
            .button("5 — админ", "/set " + to_string(eventIn.user.id) + " 5", "positive");
        eventOut.keyboard = kbd.dump();
    }
    else
    {
        uint32_t id = str::fromString(eventIn.words[1]);
        if (!id)
            eventOut.msg += "/set <id> <level>...";
        else
        {
            users::user user = users::getUser(id, eventOut.vk, false); // без инкремента msgs
            user.acess = str::fromString(eventIn.words[2]);
            users::changeUser(id, user);
            eventOut.msg += "done!";
        }
    }
}

void setCfg(Event &eventIn, Event &eventOut, Worker *worker)
{
    if (eventIn.words.size() < 2 || eventIn.words.size() > 3)
    {
        eventOut.msg += "<param> <value> or <param>";
        return;
    }
    string param = eventIn.words[1];
    conf.lock.lock();
    if (eventIn.words.size() == 2)
    {
        if (!conf.get()[param].is_null())
            eventOut.msg += conf.get()[param].dump();
        else
            eventOut.msg += "NULL";
        conf.lock.unlock();
        return;
    }
    conf.get()[param] = json::parse(eventIn.words[2]);
    conf.lock.unlock();
    conf.save();
}

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
