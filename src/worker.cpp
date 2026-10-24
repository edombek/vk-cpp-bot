#include "worker.h"
#include "bd.h"

using namespace std;

static size_t poolSizeFromConf()
{
    size_t n = 5;
    conf.lock.lock();
    if (conf.get()["pool_size"].is_number())
    {
        int v = conf.get()["pool_size"].get<int>();
        if (v > 0)
            n = static_cast<size_t>(v);
    }
    conf.lock.unlock();
    return n;
}

Worker::Worker()
    : pool(poolSizeFromConf())
{
}

void Worker::add_event(json event)
{
    // post() без future (результат не забирается) и перенос события в задачу —
    // json не копируется при постановке в очередь
    this->pool.post([this](json ev)
    {
        this->work(std::move(ev));
    }, std::move(event));
}

#include "cmd.h"
#include "keyboard.h"
#include "str.h"
#include <iostream>
thread_local Net net;
thread_local Vk vk(net);
void Worker::work(json event)
{
    this->work(event, net, vk);
}

void Worker::work(json event, Net& net, Vk& vk)
{
    try
    {
        if (event.is_null())
            return;
        Event eventIn(net, vk, event);
        if (eventIn.type == "message_event") // нажатие callback-кнопки
        {
            workButton(eventIn, vk);
            bd.touch();
            return;
        }
        bool selfComment = eventIn.type.find("wall_reply_") != std::string::npos && eventIn.from_id == eventIn.peer_id;
        // комментарий самой страницы под своей записью: игнор (защита от петли),
        // кроме комментариев чекера живых проверок с маркером «@@ » (см. tools/live_check.py --self:
        // ответы бота маркера не содержат — зацикливание исключено)
        bool selfMarker = selfComment && eventIn.msg.compare(0, 3, "@@ ") == 0;
        if (eventIn.msg.size() && eventIn.from_id != 0 && (!selfComment || selfMarker))
        {
            if (selfMarker)
            {
                eventIn.msg.erase(0, 3);
                eventIn.words = str::words(eventIn.msg);
            }
            eventIn.user = users::getUser(eventIn.from_id, vk);
            Event outEvent = eventIn.getOut();
            if (!outEvent.user.name.empty()) // у сообществ имени из users.get нет — не печатать «, »
                outEvent.msg += outEvent.user.name + ", ";
            if (cmd::start(str::low(*eventIn.words.begin()), eventIn, outEvent, this))
            {
                outEvent.send();
                // живые проверки: текст ответа в лог (wall.getComments group-токеном недоступен —
                // иначе результат комментария-команды нечем прочитать); переводы строк -> «\n»
                std::string out = outEvent.msg;
                for (char& c : out)
                    if (c == '\n')
                        c = ' ';
                if (out.size() > 300)
                    out.resize(300);
                fprintf(stderr, "\n└─> %s", out.c_str());
            }
            bd.touch(); // запись dat.json — пакетом по таймеру в Lp::loop, не на каждое сообщение
        }
    }
    catch (const std::exception& e) // исключение парсинга/скрипта/сети не роняет поток пула
    {
        std::cerr << "WORK ERROR: " << e.what() << std::endl;
    }
    catch (...)
    {
        std::cerr << "WORK ERROR: unknown exception" << std::endl;
    }
}

// нажатие callback-кнопки: payload = команда; отвечаем sendMessageEventAnswer
// (иначе клиент показывает «некорректный ответ»), затем обычные сообщения
void Worker::workButton(Event& eventIn, Vk& vk)
{
    table_t answer;
    answer["peer_id"] = std::to_string(eventIn.peer_id);
    answer["event_id"] = eventIn.event_id;
    answer["user_id"] = std::to_string(eventIn.from_id);
    answer["event_data"] = ""; // пустая строка = «действия нет», гасит спиннер кнопки;
                               // «{"type":null}» VK отвергал с ошибкой 100 (живой лог 2026-10-24)
    if (!eventIn.event_id.empty())
        vk.send("messages.sendMessageEventAnswer", answer);
    if (eventIn.from_id <= 0 || !eventIn.words.size())
        return;
    eventIn.user = users::getUser(eventIn.from_id, vk);
    Event outEvent = eventIn.getOut(); // msg пустой — без префикса имени, кнопки сами всё говорят
    if (cmd::start(str::low(*eventIn.words.begin()), eventIn, outEvent, this))
        outEvent.send();
}
