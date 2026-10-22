#pragma once
#include "common.h"
#include <mutex>

class BD
{
private:
    json dat;
    std::string file;
    bool dirty = false; // данные менялись, но ещё не записаны

public:
    BD(std::string path);
    ~BD();
    std::mutex lock;
    json& get();
    void touch(); // пометить изменённые данные (запись произойдёт позже)
    void save();  // принудительная запись
    bool saveIfDirty(); // запись только если были изменения; true == сохранили
};

extern BD conf;
extern BD bd;
