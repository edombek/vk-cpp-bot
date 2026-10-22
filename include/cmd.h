#pragma once

#include "common.h"
#include "events.h"
#include "worker.h"

namespace cmd
{
// диспетчер: поиск команды в таблице, проверка прав (eventOut.user) и вызов;
// false — команды нет или прав не хватает
bool start(const std::string &comand, Event &eventIn, Event &eventOut, Worker *worker);
}
