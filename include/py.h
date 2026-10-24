#pragma once
// Python-слой на pybind11 (фаза 2): embedded-модуль `vkcppbot`,
// каждый /py — отдельный под-интерпретатор с собственным GIL.
// PImpl: pybind11-типы скрыты в py.cpp — потребители заголовка (main.cpp, cmds.cpp)
// не зависят от pybind11 и не ловят -Wattributes (видимость типов pybind11).
#include <memory>
#include <string>

#include "events.h"

// Экземпляр создаётся в самом начале main(): инициализирует интерпретатор
// и НАВСЕГДА отпускает GIL главного потока. Это обязательно: пока главный
// поток держит GIL, создание под-интерпретатора из worker-потока умирает в deadlock
// (проверено экспериментально, docs/TESTS.md).
class PyMainThread
{
public:
    PyMainThread();
    ~PyMainThread();
    PyMainThread(const PyMainThread&) = delete;
    PyMainThread& operator=(const PyMainThread&) = delete;

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

// Один запуск скрипта /py: под-интерпретатор + изолированное пространство имён.
class pyF
{
public:
    pyF(Event& in, Event& out);
    ~pyF();
    pyF(const pyF&) = delete;
    pyF& operator=(const pyF&) = delete;

    std::string exec(std::string& code); // "" == успех, иначе traceback
    // запуск python-аддона (фаза 12): exec исходника + вызов run(eventIn, eventOut, net, vk);
    // строковый результат run() дописывается в eventOut.msg; "" == успех
    std::string execAddon(const std::string& source);
    int printsSent() const;              // сколько сообщений отправлено print'ами

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

// метаданные python-аддона (файл из py_addons/, фаза 12): exec исходника во
// временном под-интерпретаторе, чтение cmd/info/acess/cat, проверка run()
struct PyAddonMeta
{
    std::string cmd, info, cat;
    int acess = 0;
    std::string error; // "" == корректный аддон
    bool ok() const { return error.empty(); }
};
PyAddonMeta pyAddonMeta(const std::string& source);

// Текст вики по python-скриптам (команда /pywiki) — печатается print'ами по разделам
const char* pyWikiScript();
