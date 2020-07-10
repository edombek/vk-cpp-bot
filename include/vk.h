#pragma once
#include "bd.h"
#include "common.h"
#include "net.h"

#ifndef NO_PYTHON
#include<boost/python/dict.hpp>
table_t dict2map(boost::python::dict dict);
#endif // NO_PYTHON

class Vk
{
private:
    std::string token;
    std::string user_token;
    Net& net;

public:
    Vk(Net& n);
    json send(std::string method, table_t args = {}, bool user = false);
#ifndef NO_PYTHON
    std::string send(std::string method, boost::python::dict args, bool user);
#endif // NO_PYTHON
};
