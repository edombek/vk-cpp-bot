#ifndef STR_H_INCLUDED
#define STR_H_INCLUDED
#include "common.h"

namespace str {
long long int fromString(std::string s);
args_t words(const std::string& s, char delim = ' ');
std::string summ(args_t words, unsigned int s);
bool at(std::string str1, std::string str2);
std::string low(std::string str);
std::string replase(std::string str, std::string findstr, std::string replasestr);
std::string convertHtml(std::string str);
}

#endif
