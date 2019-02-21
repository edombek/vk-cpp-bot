#ifndef COMMON_H_INCLUDED
#define COMMON_H_INCLUDED

#include "json.hpp"
using json = nlohmann::json;
#include <string>
#include <vector>
typedef std::vector<std::string> args_t;
#include <map>
typedef std::map<std::string, std::string> table_t;

#include "doc.h"

#define wCount 4

#endif