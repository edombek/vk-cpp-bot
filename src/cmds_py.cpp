// Команды python-интерпретатора: /py (запуск скрипта) и /pywiki (справка по /py).
// Вынесены из cmds_admin.cpp: здесь только диспетчерские обёртки над pyF,
// сам интерпретатор и текст вики живут в py.cpp.
#include "cmds.h"
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
