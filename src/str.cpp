#include "str.h"
#include "common.h"
#include <codecvt>
#include <locale>
#include <sstream>
#include <string>

using namespace std;

#ifdef __linux__
locale const utf8("");
#elif _WIN32
locale const utf8("rus");
#endif

long long int str::fromString(string s)
{
    istringstream iss(s);
    long long int res;
    iss >> res;
    return res;
}

args_t& split(const string& s, char delim, vector<string>& elems)
{
    stringstream ss(s);
    string item;
    while (getline(ss, item, delim))
    {
        elems.push_back(item);
    }
    return elems;
}

args_t str::words(const string& s, char delim)
{
    vector<string> elems;
    split(s, delim, elems);
    return elems;
}

// Convert UTF-8 byte string to wstring
wstring to_wstring(string const& s)
{
    wstring_convert<codecvt_utf8<wchar_t>> converter {};
    return converter.from_bytes(s);
}

string to_string(wstring const& s)
{
    wstring_convert<codecvt_utf8<wchar_t>> converter {};
    return converter.to_bytes(s);
}

string str::summ(args_t words, unsigned int s)
{
    string r = "";
    if (s + 1 > words.size())
        return r;
    for (unsigned int i = s; i < words.size(); i++)
    {
        r += words[i];
        r += " ";
    }
    r.resize(r.size() - 1);
    return r;
}

bool str::at(string str1, string str2)
{
    str1 = str::low(str1);
    str2 = str::low(str2);
    return strstr(str1.c_str(), str2.c_str());
}

string str::low(string str)
{
    auto ss = to_wstring(str);
    for (auto& c : ss)
    {
        c = tolower(c, utf8);
    }
    return to_string(ss);
}

string str::replase(string str, string findstr, string replasestr)
{
    string::size_type index;
    while ((index = str.find(findstr)) != string::npos)
    {
        str.replace(index, findstr.size(), replasestr);
    }
    return str;
}

string str::convertHtml(string str)
{
    str = str::replase(str, "&amp;", "&");
    str = str::replase(str, "&quot;", "\"");
    str = str::replase(str, "&gt;", ">");
    str = str::replase(str, "&lt;", "<");
    str = str::replase(str, "»", ">>");
    str = str::replase(str, "—", "--");
    return str;
}
