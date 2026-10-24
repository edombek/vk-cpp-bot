// Семейство /apod: NASA Astronomy Picture of the Day (категория «космос»)
// Источник — WordPress-эндпоинт science.nasa.gov (legacy api.nasa.gov архивируют 01.12.2026):
//   /wp-json/wp/v2/apod-basic          — сегодня
//   /wp-json/wp/v2/apod-basic/YYMMDD   — за дату
#include "cmds.h"
#include "str.h"
#include <cstdio>
#include <ctime>
#include <random>

using namespace std;

// ===== разбор и валидация дат =====

// APOD ведутся с 16.06.1995;future-дат нет
static const long long APOD_FIRST = 19950616LL;

static bool leapYear(int y)
{
    return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0;
}

static int daysInMonth(int y, int m)
{
    static const int dm[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    if (m == 2 && leapYear(y))
        return 29;
    return dm[m - 1];
}

static bool validDate(int y, int m, int d)
{
    return y >= 1900 && y <= 2099 && m >= 1 && m <= 12 && d >= 1 && d <= daysInMonth(y, m);
}

static long long dateKey(int y, int m, int d) // сравнимое целое yyyymmdd
{
    return y * 10000LL + m * 100 + d;
}

static void todayYMD(int& y, int& m, int& d)
{
    time_t t = time(nullptr);
    struct tm lt;
    localtime_r(&t, &lt);
    y = lt.tm_year + 1900;
    m = lt.tm_mon + 1;
    d = lt.tm_mday;
}

// Форматы: ГГГГ-ММ-ДД, ДД.ММ.ГГГГ, ДД-ММ-ГГГГ, ГГГГММДД (год 95..текущий).
// 0 — дата корректна; 1 — строка не похожа на дату; 2 — синтаксис верный, но календарь такой даты не знает (30.02)
static int parseDateStr(const string& s, int& y, int& m, int& d)
{
    auto verdict = [&]() -> int
    {
        if (validDate(y, m, d))
            return 0;
        if (y >= 1900 && y <= 2099 && m >= 1 && m <= 12 && d >= 1 && d <= 31)
            return 2; // календарь такой даты не знает (30.02, 29.02 невисокосного)
        return 1;
    };
    if (s.size() == 8 && s.find_first_not_of("0123456789") == string::npos) // ГГГГММДД
    {
        y = (s[0] - '0') * 1000 + (s[1] - '0') * 100 + (s[2] - '0') * 10 + (s[3] - '0');
        m = (s[4] - '0') * 10 + (s[5] - '0');
        d = (s[6] - '0') * 10 + (s[7] - '0');
        return verdict();
    }
    int a = 0, b = 0, c = 0;
    if (sscanf(s.c_str(), "%4d-%2d-%2d", &a, &b, &c) == 3) // ISO
    {
        y = a; m = b; d = c;
        return verdict();
    }
    if (sscanf(s.c_str(), "%2d.%2d.%4d", &a, &b, &c) == 3 || sscanf(s.c_str(), "%2d-%2d-%4d", &a, &b, &c) == 3)
    {
        d = a; m = b; y = c;
        return verdict();
    }
    return 1;
}

// ===== ответ по одному APOD-объекту =====

// Формирует сообщение (заголовок/описание/кредиты) и прикладывает фото.
// Возвращает false, если объект не похож на APOD-запись (там уже и сообщение об ошибке).
static bool apodReply(Event &eventIn, Event &eventOut, const json &resp)
{
    if (!resp.is_object() || !resp["title"].is_string())
    {
        if (resp.is_object() && resp["message"].is_string())
            eventOut.msg += "APOD за эту дату не найден: " + resp["message"].get<string>();
        else
            eventOut.msg += "APOD за эту дату не найден";
        return false;
    }
    eventOut.msg += resp["title"].get<string>() + ":\n\n";
    if (resp["date"].is_string()) // для /apodrand и /apoddate полезно — дата снимка
        eventOut.msg += "(" + resp["date"].get<string>() + ")\n\n";
    if (resp["explanation"].is_string()) // новый формат присылает explanation с HTML-тегами
        eventOut.msg += str::htmlToText(resp["explanation"].get<string>());
    if (resp["credit"].is_string())
    {
        string credit = str::htmlToText(resp["credit"].get<string>());
        if (!credit.empty())
            eventOut.msg += "\n\n" + credit;
    }
    string imgurl;
    if (resp["hdurl"].is_string())
        imgurl = resp["hdurl"]; // полная картинка; url == permalink (страница поста), не картинка
    if (resp["media_type"] == "image" && !imgurl.empty())
    {
        string dat = eventIn.net.send(imgurl);
        Doc img;
        if (img.uploadPhoto("img.jpg", dat, eventIn.net, eventIn.vk, eventIn.peer_id))
            eventOut.docs.push_back(img);
        else
            eventOut.msg += "\nкартинку загрузить не удалось, первоисточник:\n" + (resp["permalink"].is_string() ? resp["permalink"].get<string>() : imgurl);
    }
    else if (resp["permalink"].is_string()) // видео/iframe/картинка без hdurl — ссылка на пост
        eventOut.msg += "\n\n" + resp["permalink"].get<string>();
    return true;
}

// GET apod-basic (путь целиком уже в url) + терпимый разбор: ответ может быть не-JSON —
// раньше json::parse бросал и /apod молчал (WORK ERROR)
static json apodFetch(Event &eventIn, const string& path)
{
    json resp = json::parse(eventIn.net.send("https://science.nasa.gov/wp-json/wp/v2/" + path), nullptr, false);
    if (resp.is_array() && !resp.empty())
        resp = resp[0]; // на будущее: эндпоинт может вернуть список
    return resp;
}

static string yymmdd(int y, int m, int d)
{
    char buf[16];
    snprintf(buf, sizeof(buf), "%02d%02d%02d", y % 100, m, d);
    return buf;
}

// ===== команды =====

void apod(Event &eventIn, Event &eventOut, Worker *worker)
{
    json resp = apodFetch(eventIn, "apod-basic");
    if (resp.is_discarded())
    {
        eventOut.msg += "NASA APOD сейчас недоступен: science.nasa.gov вернул не-JSON (HTTP-ошибка?)";
        return;
    }
    apodReply(eventIn, eventOut, resp);
}

// /apoddate [ДД.ММ.ГГГГ|ГГГГ-ММ-ДД|ДД-ММ-ГГГГ|ГГГГММДД] — APOD за дату;
// без аргумента — за день рождения из профиля VK (поле bdate)
void apoddate(Event &eventIn, Event &eventOut, Worker *worker)
{
    int y = 0, m = 0, d = 0;
    string src; // откуда дата — для текста ответа
    if (eventIn.words.size() >= 2)
    {
        int rc = parseDateStr(eventIn.words[1], y, m, d);
        if (rc == 1)
        {
            eventOut.msg += "не понимаю дату «" + eventIn.words[1] + "». Форматы: 31.10.2001, 2001-10-31, 31-10-2001, 20011031";
            return;
        }
        if (rc == 2)
        {
            char buf[96];
            snprintf(buf, sizeof(buf), "%02d.%02d.%04d — такой даты не бывает", d, m, y);
            eventOut.msg += buf;
            return;
        }
        src = "указанная дата";
    }
    else
    {
        // bdate из профиля: "д.м.гггг", "д.м" (год скрыт) или "гггг" (день/месяц скрыты)
        json us = eventIn.vk.send("users.get", { { "user_ids", std::to_string(eventIn.from_id) }, { "fields", "bdate" } });
        string bdate = us["response"][0]["bdate"].is_string() ? us["response"][0]["bdate"].get<string>() : "";
        if (bdate.empty())
        {
            eventOut.msg += "в профиле не указана дата рождения (или она скрыта).\nМожно ввести дату текстом: /apoddate 31.10.2001";
            return;
        }
        int ty, tm, td;
        todayYMD(ty, tm, td);
        bool full = false;
        if (sscanf(bdate.c_str(), "%2d.%2d.%4d", &d, &m, &y) == 3)
            full = validDate(y, m, d);
        if (!full && sscanf(bdate.c_str(), "%2d.%2d", &d, &m) != 2)
        {
            eventOut.msg += "в профиле дата рождения «" + bdate + "» без дня или месяца — введите дату текстом: /apoddate 31.10.2001";
            return;
        }
        if (!validDate(y, m, d) && !validDate(ty, m, d))
        {
            eventOut.msg += "в профиле странная дата рождения «" + bdate + "» — введите дату текстом: /apoddate 31.10.2001";
            return;
        }
        if (full && dateKey(y, m, d) < APOD_FIRST)
            full = false; // доэрастное ДР — сдвинем день/месяц на последний прошедший год
        if (!full)
        {
            // год скрыт или ДР раньше 16.06.1995 — последний прошедший год с такой датой
            y = ty;
            if (dateKey(y, m, d) > dateKey(ty, tm, td) || !validDate(y, m, d))
                y--;
            while (y >= 1995 && !validDate(y, m, d)) // 29 февраля — по високосным
                y--;
            if (y < 1995)
            {
                eventOut.msg += "не удалось подобрать год для дня рождения «" + bdate + "» — введите дату текстом: /apoddate 31.10.2001";
                return;
            }
        }
        src = "день рождения " + bdate;
    }
    int ty, tm, td;
    todayYMD(ty, tm, td);
    if (!validDate(y, m, d))
    {
        char buf[96];
        snprintf(buf, sizeof(buf), "%02d.%02d.%04d — такой даты не бывает", d, m, y);
        eventOut.msg += buf;
        return;
    }
    if (dateKey(y, m, d) > dateKey(ty, tm, td))
    {
        eventOut.msg += "APOD за будущим временем не ведутся 🙂";
        return;
    }
    if (dateKey(y, m, d) < APOD_FIRST)
    {
        eventOut.msg += "APOD ведутся только с 16.06.1995 — этой даты ещё не было.\n" + src + " можно проверить более поздним годом: /apoddate 01.01.1996";
        return;
    }
    json resp = apodFetch(eventIn, "apod-basic/" + yymmdd(y, m, d));
    if (resp.is_discarded())
    {
        eventOut.msg += "NASA APOD сейчас недоступен: science.nasa.gov вернул не-JSON (HTTP-ошибка?)";
        return;
    }
    apodReply(eventIn, eventOut, resp);
}

// /apodrand — APOD за случайный день эпохи APOD (с 16.06.1995)
void apodrand(Event &eventIn, Event &eventOut, Worker *worker)
{
    // перевод в «номер дня» и обратно (алгоритм Howard Hinnant, days_from_civil)
    auto daysFromCivil = [](int y, unsigned m, unsigned d) -> long long
    {
        y -= m <= 2;
        int era = (y >= 0 ? y : y - 399) / 400;
        unsigned yoe = static_cast<unsigned>(y - era * 400);
        unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
        unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
        return static_cast<long long>(era) * 146097 + static_cast<long long>(doe) - 719468;
    };
    auto civilFromDays = [](long long z, int& y, int& m, int& d)
    {
        z += 719468;
        long long era = (z >= 0 ? z : z - 146096) / 146097;
        unsigned doe = static_cast<unsigned>(z - era * 146097);
        unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
        int yy = static_cast<int>(yoe) + static_cast<int>(era) * 400;
        unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
        unsigned mp = (5 * doy + 2) / 153;
        d = static_cast<int>(doy - (153 * mp + 2) / 5 + 1);
        m = static_cast<int>(mp) + (mp < 10 ? 3 : -9);
        y = yy + (m <= 2);
    };
    int ty, tm, td;
    todayYMD(ty, tm, td);
    long long first = daysFromCivil(1995, 6, 16), last = daysFromCivil(ty, tm, td);
    static mt19937_64 rng(static_cast<uint64_t>(time(nullptr)) ^ 0x5bd1e995ULL);
    uniform_int_distribution<long long> dist(first, last);
    for (int attempt = 0; attempt < 10; attempt++)
    {
        int y, m, d;
        civilFromDays(dist(rng), y, m, d);
        json resp = apodFetch(eventIn, "apod-basic/" + yymmdd(y, m, d));
        if (resp.is_discarded())
        {
            eventOut.msg += "NASA APOD сейчас недоступен: science.nasa.gov вернул не-JSON (HTTP-ошибка?)";
            return;
        }
        if (apodReply(eventIn, eventOut, resp))
            return; // нашлись APOD за этот день
        eventOut.msg.clear(); // в этот день APOD не было — пробуем ещё
    }
    eventOut.msg += "не удалось подобрать случайный день с APOD — попробуй ещё раз";
}
