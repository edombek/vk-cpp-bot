#pragma once
#include "common.h"
#include "events.h"
#include "ThreadPool.h"
class Worker
{
private:
    void workButton(Event& eventIn, Vk& vk); // нажатие callback-кнопки
    ThreadPool pool;
public:
    Worker(); // размер пула — conf["pool_size"] (default 5)
    void add_event(json event);
    void work(json event); // берёт thread_local Net/Vk этого потока
    // обработчик с явной сетью: тесты подсовывают FakeNet без пула и сети
    void work(json event, Net& net, Vk& vk);
};
