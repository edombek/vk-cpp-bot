#include "bd.h"
#include "lp.h"
#include <iostream>
using namespace std;

BD conf("config.json");
BD bd("dat.json");

int main()
{
    Net::init();
    Lp lp;
    lp.loop();
    return 0;
}
