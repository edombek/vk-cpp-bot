#include "bd.h"
#include "lp.h"
#include "timer.h"
#include "worker.h"
#ifdef WIN32
#include <vld.h>
#endif
#ifndef NO_PYTHON
#include "py.h"
#endif // NO_PYTHON
using namespace std;

BD conf("config.json");
BD bd("dat.json");

int main()
{
#ifndef NO_PYTHON
    PyMainThread pymain;
#endif // NO_PYTHON
    timer::init();
    Net::init();
    Lp lp;
    lp.loop();
    Net::deinit();
    return 0;
}
