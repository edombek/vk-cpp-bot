#include "lp.h"
#include "common.h"
#include "timer.h"
#include "worker.h"
#include <iostream>

using namespace std;

Lp::Lp()
    : vk(this->net)
{
    this->vk.validate(); // сеть трогает только здесь (см. vk.h)
    this->group_id = this->vk.send("groups.getById")["response"][0]["id"];
    this->getServer();
}

void Lp::getServer()
{
    json resp = this->vk.send("groups.getLongPollServer", { { "group_id", to_string(this->group_id) } })["response"];
    this->key = resp["key"];
    this->ts = resp["ts"];
    this->server = resp["server"];
}

void Lp::loop()
{
    Worker worker;
    time_t lastSave = time(NULL);
    this->net.setTimeout(60); // a_check ждёт ответа сервера 25 с — 600-секундный дефолт только вешает цикл на мёртвом соединении
    while (true)
    {
        string buff = this->net.send(this->server, { { "act", "a_check" }, { "key", this->key }, { "ts", this->ts }, { "wait", "25" } });
        while (buff == "")
        {
            cout << "Lp: пустой ответ longpoll, повтор через 1 с" << endl;
            timer::sleep(1000);
            buff = this->net.send(this->server, { { "act", "a_check" }, { "key", this->key }, { "ts", this->ts }, { "wait", "25" } });
        }
        try
        {
            json resp = json::parse(buff);
            if (resp["failed"].is_number())
            {
                switch (resp["failed"].get<int>())
                {
                case 1:
                    this->ts = resp["ts"];
                    break;
                default:
                    this->getServer();
                    break;
                }
            }
            else
            {
                this->ts = resp["ts"];
                for (json update : resp["updates"])
                {
                    worker.add_event(std::move(update));
                }
            }
        }
        catch (const std::exception& e)
        {
            // битый JSON (или протроченный key в unexpected форме) — не роняем процесс, берём новый сервер
            cout << "Lp: разбор ответа longpoll не удался (" << e.what() << ") — обновляю сервер" << endl;
            this->getServer();
            timer::sleep(1000);
        }
        if (time(NULL) - lastSave >= 30) // периодически сбрасываем помеченные изменения bd
        {
            bd.saveIfDirty();
            lastSave = time(NULL);
        }
    }
}
