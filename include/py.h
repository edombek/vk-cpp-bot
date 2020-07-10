#pragma once

#include "common.h"

#include "net.h"
#include "vk.h"
#include "events.h"

#include <Python.h>
#include <boost/python.hpp>

#pragma comment(lib, "boost_python37-vc140-mt-x32-1_68.lib")
#pragma comment(lib, "python3.lib")

namespace py = boost::python;

class PySubThread // класс для работы в каждом порождённом потоке
{
public:
    PySubThread() // нужно для инициализации под-интерпретатора Python в самом начале работы потока
    {
        mMainGilState = PyGILState_Ensure(); // забираем блокировку основного интерпретатора
        mOldThreadState = PyThreadState_Get(); // сохраняем текущее состояние порождённого потока
        mNewThreadState = Py_NewInterpreter(); // создаём в потоке под-интерпретатор
        PyThreadState_Swap( mNewThreadState );  // с этого момента для потока актуален уже под-интерпретатор --
        mSubThreadState = PyEval_SaveThread();  // не забываем освободить предыдущую блокировку GIL --
        mSubGilState = PyGILState_Ensure();     // и заблокировать GIL уже для под-интерпретатора --
    }

    ~PySubThread() // по завершении работы потока нужно корректно освободить ресурсы под-интепретатора Python
    {
        PyGILState_Release( mSubGilState );         // разблокируем GIL для под-интерпретатора --
        PyEval_RestoreThread( mSubThreadState );    // восстанавливаем блокировку и состояние потока для основного интерпретатора --
        Py_EndInterpreter(mNewThreadState); // завершаем работу под-интерпретатора
        PyThreadState_Swap(mOldThreadState); // возвращаем состояние потока для основного интерпретатора
        PyGILState_Release(mMainGilState); // освобождаем блокировку GIL для основного интерпретатора
    }

private:
    PyGILState_STATE mMainGilState; // состояние GIL основного интерпретатора Python
    PyThreadState* mOldThreadState; // состояние текущего потока для основного интерпретатора
    PyThreadState* mNewThreadState; // состояние потока для порождённого под-интерпретатора
    PyThreadState* mSubThreadState;     // сохранённое состояние потока при разблокировке GIL
    PyGILState_STATE mSubGilState;      // состояние GIL для порождённого под-интерпретатора Python
};

class pyF
{
public:
    static void init();
    pyF(Event &in, Event &out);
    std::string error();
    std::string exec(std::string &code);
private:
    PySubThread sub;
    py::object main_module;
    py::object main_namespace;
    Event &eventIn;
    Event &eventOut;
};

class PyMainThread // специальный класс для основного потока
{
public:
    PyMainThread() // нужно создать экземпляр класса в самом начале работы
    {
        Py_Initialize(); // инициализация интерпретатора Python
        PyEval_InitThreads(); // инициализация потоков в Python и механизма GIL

        pyF::init(); // инициализация функций ядра

        mGilState = PyGILState_Ensure(); // забираем себе GIL сразу для настройки многопоточности
        mThreadState = PyEval_SaveThread(); // сохраняем состояние главного потока и отпускаем GIL

        // здесь GIL разблокирован для основного интерпретатора Python и ждёт блокировки из других потоков
    }

    ~PyMainThread() // по завершении работы нужно корректно освободит ресурсы интерпретатора
    {
        // здесь GIL должен быть разблокирован для основного интерпретатора

        PyEval_RestoreThread(mThreadState); // восстанавливаем состояние главного потока и забираем себе GIL
        PyGILState_Release(mGilState); // отпускаем блокировку GIL с сохранённым состоянием

        Py_Finalize(); // завершает работу как основного интерпретатора, со всеми под-интерпретаторами Python
    }

private:
    PyGILState_STATE mGilState; // сохранённое состояние GIL
    PyThreadState* mThreadState; // сохранённое состояние основного потока
};

py::dict tabl2map(table_t table);
table_t dict2map(py::dict dict);
