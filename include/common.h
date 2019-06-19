#pragma once
#include "json.hpp"
using json = nlohmann::json;
#include <string>
#include <vector>
typedef std::vector<std::string> args_t;
#include <map>
typedef std::map<std::string, std::string> table_t;

#define wCount 4

#define findinmap(map, q) (map).find(q) != (map).cend()
