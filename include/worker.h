#pragma once
#include "common.h"
#include <ThreadPool.h>
class Worker
{
private:
    ThreadPool pool;
public:
    Worker():pool(5) {}
    void add_event(json);
    void work(json);
};
