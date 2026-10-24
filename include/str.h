#pragma once
#include "common.h"

namespace str
{
long long int fromString(const std::string& s);
args_t words(const std::string& s, char delim = ' ');
std::string summ(const args_t& words, unsigned int s);
bool at(const std::string& str1, const std::string& str2);
std::string low(const std::string& str);
std::string replase(const std::string& str, const std::string& findstr, const std::string& replasestr);
std::string htmlToText(const std::string& html); // вырезать теги, раскрыть HTML-сущности (для новых API, напр. APOD)
}
