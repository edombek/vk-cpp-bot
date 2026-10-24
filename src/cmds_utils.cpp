// Команды-утилиты: /w (погода)
#include "cmds.h"
#include "bd.h"
#include "keyboard.h"
#include "str.h"

using namespace std;

// Погода через wttr.in (с 2026): не требует API-ключа и регистрации.
// Раньше был openweathermap — без openweathermap_token в конфиге команда только
// и делала, что обзывала админа лентяем; живые проверки упёрлись в отсутствие
// ключа. Формат j1 — структурированный JSON, lang=ru даёт русские описания.
void weather(Event &eventIn, Event &eventOut, Worker *worker)
{
    if (eventIn.words.size() < 2)
    {
        eventOut.msg += "мб город введёшь?";
        Keyboard kbd(true, false); // one_time: пропала после выбора
        kbd.button("Москва", "/w Москва").button("Санкт-Петербург", "/w Санкт-Петербург");
        kbd.row();
        kbd.button("Новосибирск", "/w Новосибирск").button("Екатеринбург", "/w Екатеринбург");
        kbd.row();
        kbd.button("Киев", "/w Киев").button("Минск", "/w Минск");
        eventOut.keyboard = kbd.dump();
        return;
    }

    // город — путём в URL («https://wttr.in/Санкт-Петербург»), кириллица/пробелы — в %-кодирование
    table_t params =
    {
        { "format", "j1" },
        { "lang", "ru" }
    };
    string cityQ = eventIn.net.urlEncode(str::summ(eventIn.words, 1));
    json wttr = json::parse(eventIn.net.send("https://wttr.in/" + cityQ, params, false), nullptr, false);
    if (wttr.is_discarded() || !wttr["current_condition"].is_array() || !wttr["current_condition"].size())
    {
        // wttr отвечает HTML-заглушкой, когда город не найден, — lenient-парсинг ловит это здесь
        eventOut.msg += "город не найден (или сервис погоды временно недоступен) — попробуй написать город по-другому";
        return;
    }
    json now = wttr["current_condition"][0]; // значения в j1 — строки («"temp_C":"10"»)

    // число из j1-поля (строка или число), 0 если мусора
    auto jnum = [](const json& o, const char* key) -> int {
        if (o[key].is_number())
            return (int)o[key].get<double>();
        if (o[key].is_string())
        {
            try
            {
                return stoi(o[key].get<string>());
            }
            catch (...) {}
        }
        return 0;
    };
    auto jdesc = [](json o) -> string { // lang_ru предпочтительнее weatherDesc (lang=ru в запросе)
        if (o["lang_ru"].is_array() && o["lang_ru"].size() && o["lang_ru"][0]["value"].is_string())
            return o["lang_ru"][0]["value"];
        if (o["weatherDesc"].is_array() && o["weatherDesc"].size() && o["weatherDesc"][0]["value"].is_string())
            return o["weatherDesc"][0]["value"];
        return "—";
    };

    string city = "—", country = "";
    if (wttr["nearest_area"].is_array() && wttr["nearest_area"].size())
    {
        json area = wttr["nearest_area"][0];
        if (area["areaName"].is_array() && area["areaName"].size() && area["areaName"][0]["value"].is_string())
            city = area["areaName"][0]["value"];
        if (area["country"].is_array() && area["country"].size() && area["country"][0]["value"].is_string())
            country = area["country"][0]["value"];
    }

    string temp = "";
    temp += "погода в " + city + (country.empty() ? "" : " (" + country + ")") + ":";
    // windspeedKmph — км/ч, приводим к привычным по старому openweathermap м/с
    temp += "\n•температура: " + to_string(jnum(now, "temp_C")) + "°C\n•скорость ветра: " + to_string((int)(jnum(now, "windspeedKmph") / 3.6 + 0.5)) + "м/с\n•влажность: " + to_string(jnum(now, "humidity")) + "%\n•описание: " + jdesc(now) + "\n";
    eventOut.msg += temp;
}
