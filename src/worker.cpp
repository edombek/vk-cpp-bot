#include "worker.h"

using namespace std;

void Worker::add_event(const json& event)
{
    this->pool.enqueue([this](json event)
    {
        this->work(event);
    }, event);
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
        if (eventIn.msg.size() && eventIn.from_id > 0)
        {
            eventIn.user = users::getUser(eventIn.from_id, vk);
            Event outEvent = eventIn.getOut();
            outEvent.msg += outEvent.user.name + ", ";
            if (cmd::start(str::low(*eventIn.words.begin()), eventIn, outEvent, this))
                outEvent.send();
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
    answer["event_data"] = "{\"type\":null}"; // актуальное имя параметра (раньше — answer)
    if (!eventIn.event_id.empty())
        vk.send("messages.sendMessageEventAnswer", answer);
    if (eventIn.from_id <= 0 || !eventIn.words.size())
        return;
    eventIn.user = users::getUser(eventIn.from_id, vk);
    Event outEvent = eventIn.getOut(); // msg пустой — без префикса имени, кнопки сами всё говорят
    if (cmd::start(str::low(*eventIn.words.begin()), eventIn, outEvent, this))
        outEvent.send();
}
