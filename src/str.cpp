#include "str.h"
#include "common.h"
#include <sstream>
#include <string>

using namespace std;

long long int str::fromString(const string& s)
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

// Декодирование одного символа UTF-8 (без <codecvt>, который deprecated в C++17).
// Возвращает кодовую точку и длину байтовой последовательности (0 — невалидный байт).
static unsigned utf8Decode(const string& s, size_t i, size_t& len)
{
    unsigned char c = (unsigned char)s[i];
    if (c < 0x80)
    {
        len = 1;
        return c;
    }
    if ((c & 0xE0) == 0xC0)
    {
        len = 2;
        unsigned cp = c & 0x1F;
        for (size_t k = 1; k < len; k++)
        {
            if (i + k >= s.size() || ((unsigned char)s[i + k] & 0xC0) != 0x80)
                return 0xFFFFFFFF;
            cp = (cp << 6) | ((unsigned char)s[i + k] & 0x3F);
        }
        return cp;
    }
    if ((c & 0xF0) == 0xE0)
    {
        len = 3;
        unsigned cp = c & 0x0F;
        for (size_t k = 1; k < len; k++)
        {
            if (i + k >= s.size() || ((unsigned char)s[i + k] & 0xC0) != 0x80)
                return 0xFFFFFFFF;
            cp = (cp << 6) | ((unsigned char)s[i + k] & 0x3F);
        }
        return cp;
    }
    if ((c & 0xF8) == 0xF0)
    {
        len = 4;
        unsigned cp = c & 0x07;
        for (size_t k = 1; k < len; k++)
        {
            if (i + k >= s.size() || ((unsigned char)s[i + k] & 0xC0) != 0x80)
                return 0xFFFFFFFF;
            cp = (cp << 6) | ((unsigned char)s[i + k] & 0x3F);
        }
        return cp;
    }
    return 0xFFFFFFFF;
}

string str::summ(const args_t& words, unsigned int s)
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

bool str::at(const string& str1, const string& str2)
{
    string s1 = str::low(str1);
    string s2 = str::low(str2);
    return strstr(s1.c_str(), s2.c_str());
}

string str::low(const string& str)
{
    // Понижается регистр ASCII и кириллицы (А–Я, Ё); остальные символы
    // проходят без изменений (прежняя реализация через wstring_convert/локаль
    // системной кодировки зависела от окружения).
    string out;
    out.reserve(str.size());
    for (size_t i = 0; i < str.size();)
    {
        size_t len = 0;
        unsigned cp = utf8Decode(str, i, len);
        if (cp == 0xFFFFFFFF) // битая последовательность — копируем байт как есть
        {
            out += str[i];
            i++;
            continue;
        }
        if (cp >= 'A' && cp <= 'Z')
            cp += 'a' - 'A';
        else if (cp >= 0x410 && cp <= 0x42F) // А–Я
            cp += 0x20;
        else if (cp == 0x401) // Ё
            cp = 0x451;
        // кодирование обратно в UTF-8
        if (cp < 0x80)
            out += (char)cp;
        else if (cp < 0x800)
        {
            out += (char)(0xC0 | (cp >> 6));
            out += (char)(0x80 | (cp & 0x3F));
        }
        else if (cp < 0x10000)
        {
            out += (char)(0xE0 | (cp >> 12));
            out += (char)(0x80 | ((cp >> 6) & 0x3F));
            out += (char)(0x80 | (cp & 0x3F));
        }
        else
        {
            out += (char)(0xF0 | (cp >> 18));
            out += (char)(0x80 | ((cp >> 12) & 0x3F));
            out += (char)(0x80 | ((cp >> 6) & 0x3F));
            out += (char)(0x80 | (cp & 0x3F));
        }
        i += len;
    }
    return out;
}

string str::replase(const string& str, const string& findstr, const string& replasestr)
{
    string out = str;
    string::size_type index;
    while ((index = out.find(findstr)) != string::npos)
    {
        out.replace(index, findstr.size(), replasestr);
    }
    return out;
}

// Кодирование кодовой точки в UTF-8 (парная к utf8Decode).
static void utf8Append(string& out, unsigned cp)
{
    if (cp < 0x80)
        out += (char)cp;
    else if (cp < 0x800)
    {
        out += (char)(0xC0 | (cp >> 6));
        out += (char)(0x80 | (cp & 0x3F));
    }
    else if (cp < 0x10000)
    {
        out += (char)(0xE0 | (cp >> 12));
        out += (char)(0x80 | ((cp >> 6) & 0x3F));
        out += (char)(0x80 | (cp & 0x3F));
    }
    else
    {
        out += (char)(0xF0 | (cp >> 18));
        out += (char)(0x80 | ((cp >> 12) & 0x3F));
        out += (char)(0x80 | ((cp >> 6) & 0x3F));
        out += (char)(0x80 | (cp & 0x3F));
    }
}

// Новый APOD (WordPress) возвращает explanation/credit с HTML-разметкой —
// вырезаем теги (<br>/<p> -> перевод строки) и раскрываем сущности (&amp;, &#8217;...).
string str::htmlToText(const string& html)
{
    static const map<string, string> named =
    {
        { "amp", "&" }, { "lt", "<" }, { "gt", ">" }, { "quot", "\"" }, { "apos", "'" },
        { "nbsp", " " }, { "copy", "©" }, { "mdash", "—" }, { "ndash", "–" }, { "hellip", "…" },
        { "ldquo", "“" }, { "rdquo", "”" }, { "lsquo", "‘" }, { "rsquo", "’" },
        { "deg", "°" }, { "micro", "µ" }, { "times", "×" }, { "laquo", "«" }, { "raquo", "»" }
    };
    string out;
    for (size_t i = 0; i < html.size();)
    {
        char c = html[i];
        if (c == '<')
        {
            size_t close = html.find('>', i);
            if (close == string::npos)
                break; // битый тег в хвосте — выкидываем
            string tag = html.substr(i + 1, close - i - 1);
            if (!tag.empty() && (str::low(tag.substr(0, 2)) == "br" || tag.substr(0, 2) == "/p" || tag.substr(0, 2) == "/d"))
                out += '\n';
            i = close + 1;
        }
        else if (c == '&')
        {
            size_t semi = html.find(';', i);
            if (semi == string::npos || semi - i > 10)
            {
                out += c;
                i++;
                continue;
            }
            string ent = html.substr(i + 1, semi - i - 1);
            auto it = named.find(ent);
            if (it != named.end())
            {
                out += it->second;
                i = semi + 1;
            }
            else if (ent.size() > 1 && ent[0] == '#')
            {
                bool hex = (ent[1] == 'x' || ent[1] == 'X');
                unsigned long cp = strtoul(ent.c_str() + (hex ? 2 : 1), nullptr, hex ? 16 : 10);
                if (cp && cp < 0x110000)
                {
                    utf8Append(out, (unsigned)cp);
                    i = semi + 1;
                }
                else
                {
                    out += c;
                    i++;
                }
            }
            else
            {
                out += c;
                i++;
            }
        }
        else
        {
            out += c;
            i++;
        }
    }
    return out;
}

