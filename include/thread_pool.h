#pragma once
// ThreadPool — часть проекта (раньше — субмодуль progschj/ThreadPool).
// Реализация — в src/thread_pool.cpp; в заголовке только класс и тонкие шаблонные
// обёртки enqueue/post (шаблоны обязаны быть в заголовке — вывод аргументов
// происходит на стороне вызывающего TU; вся тяжёлая логика — в push() из cpp).
//
// Изменён относительно оригинала (требование п.2 лицензии, текст в src/thread_pool.cpp):
//  - C++17: std::invoke_result вместо deprecated std::result_of;
//  - post() — fire-and-forget без packaged_task/future: одна аллокация std::function
//    на задачу вместо трёх (packaged_task + состояние future + обёртка); боту не нужен
//    результат (Worker::add_event ignores future);
//  - исключение задачи не убивает worker-поток пула (terminate в ранней версии),
//    оно считается в счётчике failedTasks().
// Copyright (c) 2012 Jakob Progsch, Václav Zeman — полная лицензия в src/thread_pool.cpp.

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
    ~ThreadPool();

    // задача с future (обратная совместимость с оригинальным API)
    template <class F, class... Args>
    auto enqueue(F&& f, Args&&... args) -> std::future<typename std::invoke_result<F, Args...>::type>
    {
        using return_type = typename std::invoke_result<F, Args...>::type;

        auto task = std::make_shared<std::packaged_task<return_type()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...));
        std::future<return_type> res = task->get_future();
        this->push([task] { (*task)(); });
        return res;
    }

    // fire-and-forget: без packaged_task и future — одна аллокация на задачу
    template <class F, class... Args>
    void post(F&& f, Args&&... args)
    {
        this->push(std::bind(std::forward<F>(f), std::forward<Args>(args)...));
    }

    size_t failedTasks() const; // задачи, завершившиеся исключением

private:
    void push(std::function<void()> task); // постановка под локом; кидает, если пул остановлен

    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;

    std::mutex queue_mutex;
    std::condition_variable condition;
    std::atomic<size_t> failed{0};
    bool stop = false;
};
