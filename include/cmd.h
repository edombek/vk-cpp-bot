#pragma once

#include "common.h"
#include "events.h"
#include "worker.h"

namespace cmd
{
// диспетчер: поиск команды в таблице, проверка прав (eventIn.user) и вызов;
// false — команды нет или прав не хватает
bool start(const std::string &comand, Event &eventIn, Event &eventOut, Worker *worker);

// python-аддоны (фаза 12): регистрация команд из каталога *.py; повторный вызов
// перечитывает каталог (сначала снимает ранее зарегистрированные аддоны).
// В NO_PYTHON — no-op. Каталог по умолчанию — conf["py_addons_dir"] (else "py_addons").
void loadAddons(const std::string &dir = "");
// список зарегистрированных python-команд: «/cmd — описание (ур. N, раздел)»
std::vector<std::string> addonList();
}
