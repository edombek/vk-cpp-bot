#ifndef NO_PYTHON
#include "py.h"
#endif // NO_PYTHON
#include "bd.h"
#include "cmd.h"
#include "lp.h"
#include "timer.h"
#include "worker.h"
#include <exception>
#include <iostream>
using namespace std;

BD conf("config.json");
BD bd("dat.json");

int main()
{
    try
    {
#ifndef NO_PYTHON
        PyMainThread pymain;
#endif // NO_PYTHON
        timer::init();
        Net::init();
        cmd::loadAddons(); // python-команды из py_addons/ (фаза 12; no-op в NO_PYTHON)
        Lp lp;
        lp.loop();
        Net::deinit();
    }
    catch (const std::exception& e) // внятное сообщение вместо terminate (например при битом config.json)
    {
        std::cerr << "FATAL: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
