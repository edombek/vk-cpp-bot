// Реализация ThreadPool (интерфейс и шаблонные обёртки — в include/thread_pool.h).
// Перенесён из субмодуля progschj/ThreadPool в код проекта и изменён; изменения
// помечены в заголовке. Лицензия оригинала (п.3 — не удалять и не изменять):
/*
Copyright (c) 2012 Jakob Progsch, Václav Zeman

This software is provided 'as-is', without any express or implied
warranty. In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

   1. The origin of this software must not be misrepresented; you must not
   claim that you wrote the original software. If you use this software
   in a product, an acknowledgment in the product documentation would be
   appreciated but is not required.

   2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.

   3. This notice may not be removed or altered from any source
   distribution.
*/
#include "thread_pool.h"

// конструктор запускает workers: цикл «ждать -> достать задачу -> выполнить»;
// исключение задачи не роняет поток пула (в оригинале уходило в terminate)
ThreadPool::ThreadPool(size_t threads)
{
    this->workers.reserve(threads);
    for (size_t i = 0; i < threads; ++i)
        this->workers.emplace_back(
            [this]
            {
                for (;;)
                {
                    std::function<void()> task;
                    {
                        std::unique_lock<std::mutex> lock(this->queue_mutex);
                        this->condition.wait(lock, [this] { return this->stop || !this->tasks.empty(); });
                        if (this->stop && this->tasks.empty())
                            return;
                        task = std::move(this->tasks.front());
                        this->tasks.pop();
                    }
                    try
                    {
                        task();
                    }
                    catch (...)
                    {
                        ++this->failed; // исключение задачи не роняет поток пула
                    }
                }
            });
}

// постановка задачи в очередь (из шаблонных enqueue/post)
void ThreadPool::push(std::function<void()> task)
{
    {
        std::unique_lock<std::mutex> lock(this->queue_mutex);
        if (this->stop)
            throw std::runtime_error("enqueue on stopped ThreadPool");
        this->tasks.emplace(std::move(task));
    }
    this->condition.notify_one();
}

size_t ThreadPool::failedTasks() const
{
    return this->failed.load(std::memory_order_relaxed);
}

// деструктор останавливает пул и дожидает потоков (начатые задачи дорабатываются)
ThreadPool::~ThreadPool()
{
    {
        std::unique_lock<std::mutex> lock(this->queue_mutex);
        this->stop = true;
    }
    this->condition.notify_all();
    for (std::thread& worker : this->workers)
        worker.join();
}
