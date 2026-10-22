// Команды-утилиты: /w (погода)
#include "cmds.h"
#include "bd.h"
#include "keyboard.h"
#include "str.h"

using namespace std;

#define openweathermap_msg "get token on openweathermap.org"
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

    //вытаскиваем токен от сервиса
    conf.lock.lock();
    json& c = conf.get();
    if (c["openweathermap_token"].is_null() || c["openweathermap_token"] == openweathermap_msg)
    {
        c["openweathermap_token"] = openweathermap_msg;
        conf.lock.unlock();
        conf.save();
        eventOut.msg += "передайте админу что он лентяй)";
        return;
    }
    string appid(c["openweathermap_token"].get<string>());
    conf.lock.unlock();

    table_t params =
    {
        { "lang", "ru" },
        { "units", "metric" },
        { "APPID", appid },
        { "q", str::summ(eventIn.words, 1) }
    };
    json weather = json::parse(eventIn.net.send("http://api.openweathermap.org/data/2.5/weather", params, false));
    if (weather["main"].is_null())
    {
        eventOut.msg += "чтота пошло не так, возможно надо ввести город транслитом или требуется вмешательство внешних сил(админа)";
        return;
    }
    string temp = "";
    temp += "погода в " + weather["sys"]["country"].get<string>() + "/" + weather["name"].get<string>() + ":";
    temp += "\n•температура: " + to_string((int)weather["main"]["temp"]) + "°C\n•скорость ветра: " + to_string((int)weather["wind"]["speed"]) + "м/с\n•влажность: " + to_string((int)weather["main"]["humidity"]) + "%\n•описание: " + weather["weather"][0]["description"].get<string>() + "\n";
    eventOut.msg += temp;
}
