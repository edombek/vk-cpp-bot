#ifndef THREAD_POOL_H
#define THREAD_POOL_H
// ThreadPool — часть проекта (раньше — субмодуль progschj/ThreadPool).
// Изменён относительно оригинала (требование п.2 лицензии, текст ниже):
//  - C++17: std::invoke_result вместо deprecated std::result_of;
//  - post() — fire-and-forget без packaged_task/future: одна аллокация std::function
//    на задачу вместо трёх (packaged_task + состояние future + обёртка); боту не нужен
//    результат (Worker::add_event ignores future);
//  - исключение задачи не убивает worker-поток пула (terminate в ранней версии),
//    оно считается в счётчике failedTasks().
// Copyright (c) 2012 Jakob Progsch, Václav Zeman — полная лицензия в конце файла.

#include <atomic>
#include <condition_variable>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

class ThreadPool
{
public:
    explicit ThreadPool(size_t threads);
    template <class F, class... Args>
    auto enqueue(F&& f, Args&&... args) -> std::future<typename std::invoke_result<F, Args...>::type>;
    template <class F, class... Args>
    void post(F&& f, Args&&... args); // без future; исключение задачи — в failedTasks()
    size_t failedTasks() const { return this->failed.load(std::memory_order_relaxed); }
    ~ThreadPool();

private:
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;

    std::mutex queue_mutex;
    std::condition_variable condition;
    std::atomic<size_t> failed{0};
    bool stop = false;
};

inline ThreadPool::ThreadPool(size_t threads)
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

// задача с future (обратная совместимость с оригинальным API)
template <class F, class... Args>
auto ThreadPool::enqueue(F&& f, Args&&... args) -> std::future<typename std::invoke_result<F, Args...>::type>
{
    using return_type = typename std::invoke_result<F, Args...>::type;

    auto task = std::make_shared<std::packaged_task<return_type()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...));

    std::future<return_type> res = task->get_future();
    {
        std::unique_lock<std::mutex> lock(this->queue_mutex);
        if (this->stop)
            throw std::runtime_error("enqueue on stopped ThreadPool");
        this->tasks.emplace([task] { (*task)(); });
    }
    this->condition.notify_one();
    return res;
}

// fire-and-forget: без packaged_task и future — одна аллокация на задачу
template <class F, class... Args>
void ThreadPool::post(F&& f, Args&&... args)
{
    {
        std::unique_lock<std::mutex> lock(this->queue_mutex);
        if (this->stop)
            throw std::runtime_error("post on stopped ThreadPool");
        this->tasks.emplace(std::bind(std::forward<F>(f), std::forward<Args>(args)...));
    }
    this->condition.notify_one();
}

inline ThreadPool::~ThreadPool()
{
    {
        std::unique_lock<std::mutex> lock(this->queue_mutex);
        this->stop = true;
    }
    this->condition.notify_all();
    for (std::thread& worker : this->workers)
        worker.join();
}

/* Лицензия оригинального копия (п.3 — не удалять и не изменять):

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

#endif
