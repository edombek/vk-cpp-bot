// Автоматические юнит-тесты фазы 3 (ctest, target vkbot_unit).
// Сеть почти не используется: единственный сетевой тест проверяет, что Net
// ограничивает число повторов (заведомо нескладывающийся хост .invalid).
#include "bd.h"
#include "cmd.h"
#include "doc.h"
#include "events.h"
#include "fs.h"
#include "game.h"
#include "img.h"
#include "keyboard.h"
#include "net.h"
#include "str.h"
#include "timer.h"
#include "users.h"
#include "worker.h"

#include "fakenet.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <string>

// глобалы, обычно определяемые main.cpp (нужны для линковки vk.cpp)
BD conf("u_conf.json");
BD bd("u_bd.json");

static int passed = 0;
static int failed = 0;

static void check(bool cond, const std::string& name, const std::string& detail = "")
{
    if (cond)
    {
        passed++;
        std::cout << "[ok]   " << name << std::endl;
    }
    else
    {
        failed++;
        std::cout << "[FAIL] " << name << (detail.empty() ? "" : (" -> " + detail)) << std::endl;
    }
}

// ---------------------------------------------------------------------------
// эталонная попиксельная реализация neon-преобразования (старый код img.cpp),
// нужна только чтобы сравнить с ней векторную img::neonColors
// ---------------------------------------------------------------------------
struct ref_rgb
{
    double r, g, b;
};
struct ref_hsv
{
    double h, s, v;
};

// payload кнопки VK — JSON-строка {"cmd": "..."} (требование формата keyboard)
static bool cmdPayload(const json& btn, const std::string& cmd)
{
    if (!btn["action"]["payload"].is_string())
        return false;
    json p = json::parse(btn["action"]["payload"].get<std::string>(), nullptr, false);
    return !p.is_discarded() && p.is_object() && p["cmd"] == cmd;
}

static ref_hsv ref_rgb2hsv(ref_rgb in)
{
    ref_hsv out;
    double min, max, delta;
    min = in.r < in.g ? in.r : in.g;
    min = min < in.b ? min : in.b;
    max = in.r > in.g ? in.r : in.g;
    max = max > in.b ? max : in.b;
    out.v = max;
    delta = max - min;
    if (delta < 0.00001)
    {
        out.s = 0;
        out.h = 0;
        return out;
    }
    if (max > 0.0)
        out.s = (delta / max);
    else
    {
        out.s = 0.0;
        out.h = NAN;
        return out;
    }
    if (in.r >= max)
        out.h = (in.g - in.b) / delta;
    else if (in.g >= max)
        out.h = 2.0 + (in.b - in.r) / delta;
    else
        out.h = 4.0 + (in.r - in.g) / delta;
    out.h *= 60.0;
    if (out.h < 0.0)
        out.h += 360.0;
    return out;
}

static ref_rgb ref_hsv2rgb(ref_hsv in)
{
    double hh, p, q, t, ff;
    long i;
    ref_rgb out;
    if (in.s <= 0.0)
    {
        out.r = out.g = out.b = in.v;
        return out;
    }
    hh = in.h;
    if (hh >= 360.0)
        hh = 0.0;
    hh /= 60.0;
    i = (long)hh;
    ff = hh - i;
    p = in.v * (1.0 - in.s);
    q = in.v * (1.0 - (in.s * ff));
    t = in.v * (1.0 - (in.s * (1.0 - ff)));
    switch (i)
    {
    case 0: out.r = in.v; out.g = t; out.b = p; break;
    case 1: out.r = q; out.g = in.v; out.b = p; break;
    case 2: out.r = p; out.g = in.v; out.b = t; break;
    case 3: out.r = p; out.g = q; out.b = in.v; break;
    case 4: out.r = t; out.g = p; out.b = in.v; break;
    default: out.r = in.v; out.g = p; out.b = q; break;
    }
    return out;
}

static cv::Mat refNeon(const cv::Mat& src)
{
    cv::Mat im = src.clone();
    for (int i = 0; i < im.rows; i++)
        for (int j = 0; j < im.cols; j++)
        {
            cv::Vec3b& p = im.at<cv::Vec3b>(i, j);
            if (p[0] + p[1] + p[2] == 0)
                continue;
            ref_hsv c = ref_rgb2hsv({p[2] / 255.0, p[1] / 255.0, p[0] / 255.0});
            c.v = 1;
            ref_rgb o = ref_hsv2rgb(c);
            p[0] = o.b * 255;
            p[1] = o.g * 255;
            p[2] = o.r * 255;
        }
    return im;
}

int main()
{
    Net::init();
    // герметичность: игнорируем возможные старые u_*.json от прошлых запусков
    conf.get() = json::object();
    bd.get() = json::object();

    // --- U1: Net ограничивает повторы (бывшая бесконечная рекурсия) ---
    {
        Net net;
        auto t0 = std::chrono::steady_clock::now();
        std::string r = net.send("http://no-such-host-does-not-exist.invalid/");
        auto secs = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - t0).count();
        check(r.empty(), "U1a unresolvable host -> empty reply");
        check(secs < 30, "U1b bounded retries", std::to_string(secs) + "s");
    }

    // --- U2: urlEncode без сети ---
    {
        Net net;
        check(net.urlEncode("a b=б") == "a%20b%3D%D0%B1", "U2 urlEncode", net.urlEncode("a b=б"));
    }

    // --- U3: BD dirty-флаг ---
    {
        const std::string f = "u_bd_test.json";
        remove(f.c_str());
        {
            BD db(f);
            check(!db.saveIfDirty(), "U3a clean -> no save");
            check(!fs::exists(f), "U3b no file written");
            db.get()["users"]["1"]["name"] = "тест";
            db.touch();
            check(db.saveIfDirty(), "U3c dirty -> saved");
            check(fs::exists(f), "U3d file written");
            check(!db.saveIfDirty(), "U3e clean again after save");
            db.touch();
        } // деструктор должен сбросить dirty
        std::string data = fs::readData(f);
        check(data.find("тест") != std::string::npos, "U3f destructor saved", data);
        remove(f.c_str());
    }

    // --- U4: str::low без codecvt: ASCII, кириллица, Ё, чужие скрипты, битые байты ---
    {
        check(str::low("ПРИВЕТ, WORLD!") == "привет, world!", "U4a ascii+cyrillic", str::low("ПРИВЕТ, WORLD!"));
        check(str::low("ЁЛКА Ёж") == "ёлка ёж", "U4b yo", str::low("ЁЛКА Ёж"));
        check(str::low("ΘΗΚΕ") == "ΘΗΚΕ", "U4c other scripts pass through", str::low("ΘΗΚΕ"));
        check(str::low("") == "", "U4d empty");
        std::string broken = std::string("\xC3") + "аб" + "\xE2\x28\xA1";
        std::string r = str::low(broken); // не должен падать/портить соседние символы
        check(r.find("аб") != std::string::npos, "U4e broken utf8 survives", r);
        check(str::low("ABCЁ") == "abcё", "U4f mixed", str::low("ABCЁ"));
    }

    // --- U5: str regression (words/summ/replase) ---
    {
        args_t w = str::words("раз два три");
        check(w.size() == 3 && w[1] == "два", "U5a words");
        check(str::summ(w, 1) == "два три", "U5b summ", str::summ(w, 1));
        check(str::replase("а-а-а", "-", "+") == "а+а+а", "U5c replase");
    }

    // --- U6: fs roundtrip ---
    {
        const std::string f = "u_fs_test.txt";
        fs::writeData(f, "данные\n");
        check(fs::readData(f) == "данные\n", "U6a fs roundtrip");
        check(fs::exists(f) && !fs::exists(f + "XX"), "U6b fs exists");
        remove(f.c_str());
    }

    // --- U7: векторный neonColors == старый попиксельный цикл (±1 LSB) + бенчмарк ---
    {
        cv::Mat src(240, 320, CV_8UC3);
        cv::RNG rng(42);
        for (int i = 0; i < src.rows; i++)
            for (int j = 0; j < src.cols; j++)
            {
                int mode = (i * src.cols + j) % 4;
                if (mode == 0)
                    src.at<cv::Vec3b>(i, j) = cv::Vec3b(0, 0, 0); // чёрный (пропускается)
                else if (mode == 1)
                {
                    uchar g = uchar(i * 255 / src.rows); //серое (грейдскейл)
                    src.at<cv::Vec3b>(i, j) = cv::Vec3b(g, g, g);
                }
                else if (mode == 2)
                    src.at<cv::Vec3b>(i, j) = cv::Vec3b(uchar(j), uchar(i), uchar((i + j) & 0xFF));
                else
                    src.at<cv::Vec3b>(i, j) = cv::Vec3b(uchar(rng.uniform(0, 255)), uchar(rng.uniform(0, 255)), uchar(rng.uniform(0, 255)));
            }
        cv::Mat vec = img::neonColors(src);
        cv::Mat ref = refNeon(src);
        cv::Mat diff;
        cv::absdiff(vec, ref, diff);
        double maxd = 0;
        cv::minMaxLoc(diff, nullptr, &maxd);
        check(maxd <= 2.0, "U7a neonColors == reference", "max diff " + std::to_string((int)maxd));

        // бенчмарк на большом кадре — печатается для docs/TESTS.md;
        // минимум из 3 прогонов, чтобы CI-шум не ронял проверку U7b
        cv::Mat big(720, 1280, CV_8UC3);
        cv::randu(big, 0, 255);
        long long tVec = 1000000, tRef = 1000000;
        for (int rep = 0; rep < 3; rep++)
        {
            auto t0 = std::chrono::steady_clock::now();
            img::neonColors(big);
            long long dt = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
            if (dt < tVec) tVec = dt;
            t0 = std::chrono::steady_clock::now();
            refNeon(big);
            dt = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
            if (dt < tRef) tRef = dt;
        }
        std::cout << "[info] neon 1280x720: scalar=" << tRef << "ms vector=" << tVec << "ms speedup x"
                  << (tVec ? double(tRef) / double(tVec) : 0.0) << std::endl;
        check(tVec < tRef, "U7b vector faster than scalar", std::to_string(tRef) + " -> " + std::to_string(tVec) + "ms");
    }

    // --- U8: Keyboard — формат JSON-клавиатуры VK ---
    {
        Keyboard kbd(false, false);
        kbd.button("да", "/yes", "positive").button("нет", "/no");
        kbd.row();
        kbd.link("сайт", "https://example.com").textButton("текст");
        json j = json::parse(kbd.dump());
        check(j["one_time"] == false && j["inline"] == false, "U8a flags");
        check(j["buttons"].size() == 2 && j["buttons"][0].size() == 2 && j["buttons"][1].size() == 2, "U8b rows");
        check(j["buttons"][0][0]["action"]["type"] == "callback" &&
              json::parse(j["buttons"][0][0]["action"]["payload"].get<std::string>())["cmd"] == "/yes" &&
              j["buttons"][0][0]["action"]["payload"].is_string() && // VK: payload — JSON-строка в скобках
              j["buttons"][0][0]["color"] == "positive",
              "U8c callback button", j["buttons"][0][0].dump());
        check(j["buttons"][1][0]["action"]["type"] == "openlink" && j["buttons"][1][0]["action"]["link"] == "https://example.com", "U8d link button");
        check(j["buttons"][1][1]["action"]["type"] == "text", "U8e text button");
        // VK отвергает пустые ряды и ряды из >5 кнопок (error 911) — билдер чинит сам
        Keyboard k2;
        k2.button("a", "/a").row(); // ряд в конце заведомо пустой
        json j2 = json::parse(k2.dump());
        check(j2["buttons"].size() == 1, "U8f empty row cut", j2.dump());
        Keyboard k3;
        for (int i = 0; i < 7; i++) k3.button(std::to_string(i), "/b" + std::to_string(i));
        json j3 = json::parse(k3.dump());
        check(j3["buttons"].size() == 2 && j3["buttons"][0].size() == 5 && j3["buttons"][1].size() == 2,
              "U8g auto row wrap (>5)", j3.dump());
    }

    // --- U9: парсинг message_event (нажатие callback-кнопки) ---
    {
        Net net;
        Vk vk(net);
        json ev =
        {
            { "type", "message_event" },
            { "object",
              {
                  { "peer_id", 2000000005 },
                  { "user_id", 777 },
                  { "event_id", "abc-123" },
                  { "payload", "/help игры" }
              } }
        };
        Event e(net, vk, ev);
        check(e.from_id == 777 && e.peer_id == 2000000005, "U9a ids");
        check(e.event_id == "abc-123", "U9b event_id");
        check(e.msg == "/help игры" && e.words.size() == 2, "U9c payload as msg");
        check(e.is_chat, "U9d is_chat");
        // payload в формате VK: JSON-объект {"cmd": "..."}
        json ev2 =
        {
            { "type", "message_event" },
            { "object",
              {
                  { "peer_id", 111 },
                  { "user_id", 777 },
                  { "event_id", "x1" },
                  { "payload", { { "cmd", "/stat" } } }
              } }
        };
        Event e2(net, vk, ev2);
        check(e2.msg == "/stat", "U9e object payload {cmd}", e2.msg);
    }

    // --- U23: message_new без текста (text: null) и вложения с неполными полями ---
    // падали в живой проверке: WORK ERROR type_error.302 на text:null; photo без sizes — UB на ext
    {
        Net net;
        Vk vk(net);
        json ev =
        {
            { "type", "message_new" },
            { "object",
              {
                  { "id", 1 },
                  { "date", 1700000000 },
                  { "peer_id", 111 },
                  { "from_id", 111 },
                  { "text", nullptr },
                  { "random_id", 0 },
                  { "attachments",
                    {
                        { { "type", "photo" },
                          { "photo", { { "id", 5 }, { "owner_id", -1 }, { "sizes", json::array() } } } },
                        { { "type", "audio_message" },
                          { "audio_message", { { "id", 6 }, { "owner_id", -1 } } } }
                    } }
              } }
        };
        Event e(net, vk, ev);
        check(!e.msg.size() && e.words.empty(), "U23a text:null -> пустой msg");
        check(e.docs.size() == 2, "U23b docs parsed", std::to_string(e.docs.size()));
        check(e.docs[0].url.empty() && e.docs[0].ext.empty(), "U23c photo без sizes без UB");
        check(e.docs[1].ext == "mp3" && e.docs[1].url.empty(), "U23d audio_message без link_mp3");
    }

    // --- U24: message_new в формате API >= 5.103 (object = { message, client_info }) ---
    {
        Net net;
        Vk vk(net);
        json ev =
        {
            { "type", "message_new" },
            { "object",
              {
                  { "client_info",
                    {
                        { "button_actions", { "text", "callback" } },
                        { "keyboard", true },
                        { "inline_keyboard", true },
                        { "carousel", false },
                        { "lang_id", 0 }
                    } },
                  { "message",
                    {
                        { "id", 2 },
                        { "date", 1700000001 },
                        { "peer_id", 111 },
                        { "from_id", 111 },
                        { "text", "/help" },
                        { "random_id", 0 },
                        { "attachments",
                          { { { "type", "audio_message" },
                              { "audio_message", { { "id", 7 }, { "owner_id", -1 }, { "link_mp3", "https://x/y.mp3" } } } } } }
                    } }
              } }
        };
        Event e(net, vk, ev);
        check(e.msg == "/help" && e.from_id == 111 && e.peer_id == 111, "U24a fields from message wrapper");
        check(!e.is_chat && e.timestamp == 1700000001, "U24b is_chat/timestamp");
        check(e.docs.size() == 1 && e.docs[0].url == "https://x/y.mp3", "U24c attachments inside wrapper", std::to_string(e.docs.size()));
    }

    // --- U10: Event::send передаёт keyboard в messages.send (FakeNet) ---
    {
        FakeNet fn;
        fn.responses["messages.send"] = "{\"response\":10}";
        Vk vk(fn);
        Event e(fn, vk);
        e.type = "message_new";
        e.peer_id = 1;
        e.msg = "x";
        Keyboard kbd(true, false);
        kbd.button("кнопка", "/pressed");
        e.keyboard = kbd.dump();
        e.send();
        const FakeNet::Call* c = fn.first("messages.send");
        check(c != nullptr, "U10a messages.send called");
        check(c && c->params.count("keyboard") && json::parse(c->params.at("keyboard"))["one_time"] == true,
              "U10b keyboard param present", c ? c->params.count("keyboard") ? c->params.at("keyboard") : "(нет)" : "(нет вызова)");
    }

    // --- U11: полный конвейер message_event: FakeNet + Worker (+ rename) ---
    {
        FakeNet fn;
        fn.responses["users.get"] = "{\"response\":[{\"id\":777,\"first_name\":\"Кнопка\"}]}";
        fn.responses["messages.sendMessageEventAnswer"] = "{\"response\":1}";
        fn.responses["messages.send"] = "{\"response\":11}";
        Vk vk(fn);
        json ev =
        {
            { "type", "message_event" },
            { "object",
              {
                  { "peer_id", 2000000005 },
                  { "user_id", 777 },
                  { "event_id", "ev-777" },
                  { "payload", "/rename Тест" }
              } }
        };
        Worker w;
        w.work(ev, fn, vk);
        const FakeNet::Call* ans = fn.first("sendMessageEventAnswer");
        check(ans != nullptr && ans->params.count("event_id") && ans->params.at("event_id") == "ev-777" && ans->params.at("user_id") == "777",
              "U11a sendMessageEventAnswer", ans ? ans->params.count("event_id") ? ans->params.at("event_id") : "(нет event_id)" : "(нет вызова)");
        const FakeNet::Call* msg = fn.firstExact("https://api.vk.ru/method/messages.send");
        check(msg != nullptr && msg->params.count("message") && msg->params.at("message").find("done!") != std::string::npos,
              "U11b команда выполнена", msg ? msg->params.count("message") ? msg->params.at("message") : "(пусто)" : "(нет вызова)");
        check(bd.get()["users"]["777"]["name"] == "Тест", "U11c rename применился", bd.get()["users"]["777"]["name"].dump());
        check(ans != nullptr && ans->params.count("event_data") && !ans->params.count("answer"),
              "U11d event_data вместо answer", ans && ans->params.count("event_data") ? ans->params.at("event_data") : "(нет)");
    }

    // --- U12: /help по message → ответ с клавиатурой разделов ---
    {
        FakeNet fn;
        fn.responses["users.get"] = "{\"response\":[{\"id\":777,\"first_name\":\"Кнопка\"}]}";
        fn.responses["messages.send"] = "{\"response\":12}";
        Vk vk(fn);
        json ev =
        {
            { "type", "message_new" },
            { "object",
              {
                  { "date", 100 },
                  { "peer_id", 777 },
                  { "from_id", 777 },
                  { "text", "/help" },
                  { "random_id", 1 },
                  { "attachments", json::array() }
              } }
        };
        Worker w;
        w.work(ev, fn, vk);
        const FakeNet::Call* msg = fn.firstExact("https://api.vk.ru/method/messages.send");
        check(msg != nullptr && msg->params.count("message") && msg->params.at("message").find("уровень прав") != std::string::npos,
              "U12a help отправлен", msg ? msg->params.count("message") ? msg->params.at("message") : "(нет message)" : "(нет вызова)");
        // верхний уровень справки — кнопки разделов; проверяем наличие раздела «игры» (там /bc)
        bool kbdOk = false;
        if (msg && msg->params.count("keyboard"))
        {
            json j = json::parse(msg->params.at("keyboard"));
            for (const auto& row : j["buttons"])
                for (const auto& b : row)
                    if (cmdPayload(b, "/help игры"))
                        kbdOk = true;
        }
        check(kbdOk, "U12b help keyboard (разделы)", msg && msg->params.count("keyboard") ? msg->params.at("keyboard") : "(нет)");
        check(msg && msg->params.count("message") &&
              msg->params.at("message").find("поделены на разделы") != std::string::npos &&
              msg->params.at("message").find("игры —") != std::string::npos &&
              msg->params.at("message").find("текстом: /интерфейс /утилиты /игры /медиа") != std::string::npos &&
              msg->params.at("message").find("/всекоманды") != std::string::npos,
              "U12c текст о подгруппах и текстовых ярлыках", msg && msg->params.count("message") ? msg->params.at("message") : "(нет)");
        fn.reset();
        json ev2 = ev;
        ev2["object"]["text"] = "/help все";
        w.work(ev2, fn, vk);
        const FakeNet::Call* all = fn.firstExact("https://api.vk.ru/method/messages.send");
        bool allOk = all && all->params.count("message") &&
                     all->params.at("message").find("[ игры ]") != std::string::npos &&
                     all->params.at("message").find("/bc") != std::string::npos &&
                     all->params.at("message").find("или текстом: /интерфейс") != std::string::npos &&
                     all->params.at("message").find("минимальный уровень прав") != std::string::npos && // расшифровка «(ур. N)»
                     all->params.at("message").find(" (ур. ") == std::string::npos && // пометок уровня для юзера (1) нет
                     all->params.at("message").find("/py ") == std::string::npos &&
                     all->params.at("message").find("/py\n") == std::string::npos; // недоступное /py не показывается (/pywiki — можно, у него доступ 1)
        check(allOk, "U12d /help все — команды по разделам", all && all->params.count("message") ? all->params.at("message") : "(нет)");
        fn.reset();
        json ev3 = ev;
        ev3["object"]["text"] = "/игры";
        w.work(ev3, fn, vk);
        const FakeNet::Call* sec = fn.firstExact("https://api.vk.ru/method/messages.send");
        bool secOk = sec && sec->params.count("message") &&
                     sec->params.at("message").find("раздел: игры") != std::string::npos &&
                     sec->params.at("message").find("/bc") != std::string::npos &&
                     sec->params.at("message").find("работают и текстом: /интерфейс /утилиты /игры /медиа") != std::string::npos &&
                     sec->params.at("message").find("полный список — /всекоманды") != std::string::npos;
        check(secOk, "U12e ярлык /игры == /help игры", sec && sec->params.count("message") ? sec->params.at("message") : "(нет)");
        fn.reset();
        json ev4 = ev;
        ev4["object"]["text"] = "/всекоманды";
        w.work(ev4, fn, vk);
        const FakeNet::Call* vc = fn.firstExact("https://api.vk.ru/method/messages.send");
        bool vcOk = vc && vc->params.count("message") &&
                    vc->params.at("message").find("[ игры ]") != std::string::npos &&
                    vc->params.at("message").find(", все команды") != std::string::npos;
        check(vcOk, "U12f ярлык /всекоманды — полный список", vc && vc->params.count("message") ? vc->params.at("message") : "(нет)");
    }

    // --- U13: парсинг message_new: вложения (photo/doc), пересылки, беседа ---
    {
        Net net;
        Vk vk(net);
        json photoAttach =
        {
            { "type", "photo" },
            { "photo",
              {
                  { "id", 5 }, { "owner_id", -1 }, { "access_key", "ak" }, { "date", 10 },
                  { "sizes",
                    {
                        { { "width", 100 }, { "height", 100 }, { "url", "http://x/small.jpg" } },
                        { { "width", 200 }, { "height", 200 }, { "url", "http://x/big.jpg" } }
                    } }
              } }
        };
        json docAttach = { { "type", "doc" }, { "doc", { { "id", 9 }, { "owner_id", 2 }, { "access_key", "dk" }, { "ext", "txt" }, { "date", 1 }, { "url", "http://d/f.txt" } } } };
        json attach = json::array();
        attach.push_back(photoAttach);
        attach.push_back(docAttach);
        json ev =
        {
            { "type", "message_new" },
            { "object",
              {
                  { "date", 100 }, { "peer_id", 2000000001 }, { "from_id", 42 }, { "text", "привет мир" },
                  { "random_id", 7 }, { "attachments", attach },
                  { "fwd_messages", json::array({ { { "date", 90 }, { "from_id", 42 }, { "text", "первоисточник" }, { "peer_id", 2000000001 } } }) }
              } }
        };
        Event e(net, vk, ev);
        check(e.words.size() == 2 && e.is_chat, "U13a text/chat");
        check(e.docs.size() == 2, "U13b attachments", std::to_string(e.docs.size()));
        check(e.docs.size() == 2 && e.docs[0].type == "photo" && e.docs[0].url == "http://x/big.jpg" && e.docs[0].ext == "jpg",
              "U13c biggest photo", e.docs.size() ? e.docs[0].url : "");
        check(e.docs.size() == 2 && e.docs[1].get() == "doc2_9_dk", "U13d doc get", e.docs.size() ? e.docs[1].get() : "");
        check(e.fwds.size() == 1 && e.fwds[0].msg == "первоисточник", "U13e forwards");
    }

    // --- U14: wall_post_new → ответ через wall.createComment ---
    {
        FakeNet fn;
        fn.responses["wall.createComment"] = "{\"response\":1}";
        Vk vk(fn);
        json ev =
        {
            { "type", "wall_post_new" },
            { "object", { { "from_id", 42 }, { "date", 5 }, { "text", "/stat" }, { "id", 3 }, { "owner_id", -1 } } }
        };
        Event e(fn, vk, ev);
        check(e.peer_id == -1 && e.id == 3, "U14a wall ids"); // owner_id сообщества знаковый: wall.createComment ждёт «-1», не 2^32-1
        e.msg = "коммент";
        e.send();
        const FakeNet::Call* c = fn.first("wall.createComment");
        check(c && c->params.count("post_id") && c->params.at("post_id") == "3", "U14b wall comment sent", c ? c->params.count("post_id") ? c->params.at("post_id") : "?" : "(нет вызова)");
    }

    // --- U15: диспетчер cmd::start: неизвестная команда, права, выполнение ---
    {
        FakeNet fn;
        Vk vk(fn);
        Event in(fn, vk), out(fn, vk);
        in.user = out.user = users::user { "U", 1, 1, 0 };
        check(!cmd::start("/нет-такой", in, out, nullptr), "U15a unknown command");
        check(!cmd::start("/con", in, out, nullptr), "U15b access denied (con, acess 1)");
        out.msg.clear();
        check(cmd::start("/stat", in, out, nullptr), "U15c /stat dispatched");
        check(out.msg.find("Работаю") != std::string::npos, "U15d /stat output", out.msg.substr(0, 80));
    }

    // --- U16: users::getUser (новый пользователь через users.get), инкремент, changeUser ---
    {
        FakeNet fn;
        fn.responses["users.get"] = "{\"response\":[{\"id\":555,\"first_name\":\"Пятый\"}]}";
        Vk vk(fn);
        users::user u = users::getUser(555, vk);
        check(u.name == "Пятый" && u.acess == 1 && u.msgs == 0, "U16a newUser", u.name + "/" + std::to_string(u.acess));
        users::getUser(555, vk); // inc=true по умолчанию
        check(bd.get()["users"]["555"]["msgs"] == 2, "U16b msgs increment", bd.get()["users"]["555"]["msgs"].dump());
        users::getUser(555, vk, false);
        check(bd.get()["users"]["555"]["msgs"] == 2, "U16c no increment with inc=false");
        u.acess = 3;
        users::changeUser(555, u);
        check(bd.get()["users"]["555"]["acess"] == 3, "U16d changeUser");
    }

    // --- U17: Doc get() без access_key и пустой Doc ---
    {
        Doc d;
        check(d.get() == "_0_0" || d.get() == "0_0", "U17a empty doc get", d.get());
        json v = { { "type", "video" }, { "video", { { "id", 1 }, { "owner_id", 2 }, { "date", 3 } } } };
        Doc vd(v);
        check(vd.get() == "video2_1", "U17b video get", vd.get());
    }

    // --- U18: game: стартовое состояние, ход, смена игрока, формат карты ---
    {
        game g;
        check(g.user == 1, "U18a first move user");
        check(!g.isWin(), "U18b not win at start");
        g.gameUplevel(1, 1);
        check(!g.isWin(), "U18c still playing after move");
        std::string map = g.nexStep();
        check(g.user == 0, "U18d turn switched");
        check(map.find("⭕") != std::string::npos && map.find("1⃣") != std::string::npos, "U18e map rendered", map.substr(0, 20));
    }

    // --- U19: /bc через кнопки: старт, набор цифры, проверка (FakeNet+Worker) ---
    {
        FakeNet fn;
        fn.responses["users.get"] = "{\"response\":[{\"id\":333,\"first_name\":\"Игрок\"}]}";
        fn.responses["/method/messages."] = "{\"response\":1}";
        Vk vk(fn);
        auto lastMsg = [&fn]() -> std::string
        {
            const FakeNet::Call* c = nullptr;
            for (const auto& x : fn.calls)
                if (x.url == "https://api.vk.ru/method/messages.send")
                    c = &x;
            return c && c->params.count("message") ? c->params.at("message") : "(нет)";
        };
        auto lastKbdButtons = [&fn]() -> int
        {
            const FakeNet::Call* c = nullptr;
            for (const auto& x : fn.calls)
                if (x.url == "https://api.vk.ru/method/messages.send" && x.params.count("keyboard"))
                    c = &x;
            if (!c)
                return -1;
            return (int)json::parse(c->params.at("keyboard"))["buttons"].size();
        };
        auto sendMsg = [&](const std::string& text)
        {
            fn.reset();
            json ev = { { "type", "message_new" },
                        { "object", { { "date", 1 }, { "peer_id", 333 }, { "from_id", 333 }, { "text", text }, { "random_id", 1 } } } };
            Worker w;
            w.work(ev, fn, vk);
        };
        auto pressBtn = [&](const std::string& payload)
        {
            fn.reset();
            json ev = { { "type", "message_event" },
                        { "object", { { "peer_id", 333 }, { "user_id", 333 }, { "event_id", "b" + payload }, { "payload", payload } } } };
            Worker w;
            w.work(ev, fn, vk);
        };
        sendMsg("/bc");
        check(lastMsg().find("цель данной игры") != std::string::npos, "U19a bc intro", lastMsg().substr(0, 60));
        check(lastKbdButtons() == 3, "U19b bc keyboard rows (5+5+3)", std::to_string(lastKbdButtons()));
        for (char d : std::string("1234"))
            pressBtn(std::string("/bc num ") + d);
        check(lastMsg().find("ввод: 1234 [4/4]") != std::string::npos, "U19c digit draft", lastMsg());
        pressBtn("/bc go");
        bool guess = lastMsg().find("найдено") != std::string::npos || lastMsg().find("отгадал") != std::string::npos;
        check(guess, "U19d check submitted", lastMsg());
    }

    // --- U20: /w без аргумента — кнопки городов; /w Москва — разбор погоды wttr.in (FakeNet) ---
    {
        FakeNet fn;
        fn.responses["users.get"] = "{\"response\":[{\"id\":444,\"first_name\":\"Город\"}]}";
        fn.responses["wttr.in"] =
            "{\"current_condition\":[{\"temp_C\":\"-5\",\"windspeedKmph\":\"15\",\"humidity\":\"90\","
            "\"lang_ru\":[{\"value\":\"метель\"}],\"weatherDesc\":[{\"value\":\"Snow\"}]}],"
            "\"nearest_area\":[{\"areaName\":[{\"value\":\"Москва\"}],\"country\":[{\"value\":\"Россия\"}]}]}";
        fn.responses["/method/messages."] = "{\"response\":1}"; // ключ с /method/ — иначе матчит и wttr
        Vk vk(fn);
        auto sendMsg = [&](const std::string& text)
        {
            fn.reset();
            json ev = { { "type", "message_new" },
                        { "object", { { "date", 1 }, { "peer_id", 444 }, { "from_id", 444 }, { "text", text }, { "random_id", 1 } } } };
            Worker w;
            w.work(ev, fn, vk);
            const FakeNet::Call* c = nullptr;
            for (const auto& x : fn.calls)
                if (x.url == "https://api.vk.ru/method/messages.send")
                    c = &x;
            return c ? std::make_pair(c->params.count("message") ? c->params.at("message") : std::string("(нет)"),
                                      static_cast<bool>(c->params.count("keyboard"))) : std::make_pair(std::string("(нет вызова)"), false);
        };
        auto r1 = sendMsg("/w");
        check(r1.first.find("город введёшь") != std::string::npos && r1.second, "U20a weather buttons", r1.first);
        bool hasMoscow = false;
        const FakeNet::Call* c1 = nullptr;
        for (const auto& x : fn.calls)
            if (x.url == "https://api.vk.ru/method/messages.send" && x.params.count("keyboard"))
                c1 = &x;
        if (c1)
        {
            json kbd = json::parse(c1->params.at("keyboard")); // именованная переменная: временный объект live-time
            for (const auto& row : kbd["buttons"])
                for (const auto& b : row)
                    if (cmdPayload(b, "/w Москва"))
                        hasMoscow = true;
        }
        check(hasMoscow, "U20b /w Москва payload");
        auto r2 = sendMsg("/w Москва");
        check(r2.first.find("погода в Москва (Россия)") != std::string::npos && r2.first.find("-5°C") != std::string::npos &&
              r2.first.find("4м/с") != std::string::npos && r2.first.find("метель") != std::string::npos,
              "U20c weather parsed", r2.first.substr(0, 80));
        const FakeNet::Call* wc = fn.first("wttr.in");
        check(wc && wc->url.find("wttr.in/%D0%9C%D0%BE%D1%81%D0%BA%D0%B2%D0%B0") != std::string::npos,
              "U20d город в URL %-закодирован", wc ? wc->url : std::string("(нет вызова)"));
        // город не найден: wttr вернул не-JSON (HTML) —lenient-парсинг, сообщение вместо молчания/падения
        FakeNet fn2;
        fn2.responses["users.get"] = "{\"response\":[{\"id\":444,\"first_name\":\"Город\"}]}";
        fn2.responses["wttr.in"] = "<html>unknown location</html>";
        fn2.responses["/method/messages."] = "{\"response\":1}";
        Vk vk2(fn2);
        json ev2 = { { "type", "message_new" },
                     { "object", { { "date", 1 }, { "peer_id", 444 }, { "from_id", 444 }, { "text", "/w Кудыкина-гора" }, { "random_id", 1 } } } };
        Worker w2;
        w2.work(ev2, fn2, vk2);
        const FakeNet::Call* c2 = nullptr;
        for (const auto& x : fn2.calls)
            if (x.url == "https://api.vk.ru/method/messages.send")
                c2 = &x;
        check(c2 && c2->params.count("message") && c2->params.at("message").find("город не найден") != std::string::npos,
              "U20e неизвестный город — объяснение", c2 ? c2->params.count("message") ? c2->params.at("message") : std::string("(без msg)") : std::string("(нет вызова)"));
    }

    // --- U21: /set без аргументов — кнопки уровней (для админа) ---
    {
        FakeNet fn;
        fn.responses["users.get"] = "{\"response\":[{\"id\":888,\"first_name\":\"Админ\"}]}";
        fn.responses["messages."] = "{\"response\":1}";
        Vk vk(fn);
        users::user adm = users::getUser(888, vk);
        adm.acess = 5;
        users::changeUser(888, adm);
        json ev = { { "type", "message_new" },
                    { "object", { { "date", 1 }, { "peer_id", 888 }, { "from_id", 888 }, { "text", "/set" }, { "random_id", 1 } } } };
        Worker w;
        w.work(ev, fn, vk);
        const FakeNet::Call* c = nullptr;
        for (const auto& x : fn.calls)
            if (x.url == "https://api.vk.ru/method/messages.send")
                c = &x;
        bool hasAdmBtn = false;
        if (c && c->params.count("keyboard"))
        {
            json kbd = json::parse(c->params.at("keyboard"));
            for (const auto& row : kbd["buttons"])
                for (const auto& b : row)
                    if (cmdPayload(b, "/set 888 5"))
                        hasAdmBtn = true;
        }
        check(hasAdmBtn, "U21 set level buttons", c ? "(есть ответ)" : "(нет вызова)");
    }

    // --- U22: игра /g в беседе: создание, вход, ходы, стоп (Worker+FakeNet) ---
    {
        FakeNet fn;
        fn.responses["users.get"] = "{\"response\":[{\"id\":0,\"first_name\":\"Игрок\"}]}";
        fn.responses["/method/messages."] = "{\"response\":1}";
        Vk vk(fn);
        auto gmsg = [&](uint32_t from, const std::string& text) -> std::string
        {
            fn.reset();
            json ev = { { "type", "message_new" },
                        { "object", { { "date", 1 }, { "peer_id", 2000000005u }, { "from_id", from }, { "text", text }, { "random_id", 1 } } } };
            Worker w;
            w.work(ev, fn, vk);
            std::string all; // /g шлёт два сообщения (карту и «Ходит…») — склеиваем
            for (const auto& x : fn.calls)
                if (x.url == "https://api.vk.ru/method/messages.send" && x.params.count("message"))
                    all += x.params.at("message");
            return all;
        };
        check(gmsg(111, "/g").find("ожидаем второго игрока") != std::string::npos, "U22a game created");
        check(gmsg(111, "/g").find("нельзя создать игру с самим собой") != std::string::npos, "U22b self-game removed");
        check(gmsg(111, "/g").find("ожидаем второго игрока") != std::string::npos, "U22c created again");
        std::string m4 = gmsg(222, "/g"); // второй игрок входит — сразу карта, nexStep передаёт ход создателю
        check(m4.find("1⃣") != std::string::npos && m4.find("Ходит @id111") != std::string::npos, "U22d join shows map", m4.substr(0, 60));
        check(gmsg(222, "/g 1 1").find("не твой ход") != std::string::npos, "U22e not your turn");
        std::string m6 = gmsg(111, "/g 1 1"); // ход создателя
        check(m6.find("1⃣") != std::string::npos && m6.find("Ходит @id222") != std::string::npos, "U22f move + map", m6.substr(0, 60));
        check(gmsg(222, "/g").find("кординату введи") != std::string::npos, "U22g need coords");
        check(gmsg(222, "/g stop").find("игра остановлена") != std::string::npos, "U22h stop");
    }

    // --- U25: Net setTimeout — запрос на «чёрную дыру» обрезается таймаутом (longpoll-фикс) ---
    {
        Net net;
        net.setTimeout(1); // 10.255.255.1: соединение висит, CURLOPT_TIMEOUT должен обрезать
        auto t0 = std::chrono::steady_clock::now();
        std::string r = net.send("http://10.255.255.1/");
        auto secs = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - t0).count();
        check(r.empty(), "U25a blackhole host -> пустой ответ");
        check(secs < 20, "U25b setTimeout сработал (не ждать 600 c)", std::to_string(secs) + "s");
    }

    // --- U26: id новых диалогов с ботом (~2*10^11) не обрезаются до 32 бит (регресс 2026, найдено живой проверкой) ---
    {
        FakeNet fn;
        fn.responses["users.get"] = "{\"response\":[{\"id\":200006334752,\"first_name\":\"Гигант\"}]}";
        fn.responses["messages.send"] = "{\"response\":26}";
        Vk vk(fn);
        json ev =
        {
            { "type", "message_new" },
            { "object",
              {
                  { "message",
                    {
                        { "id", 18683 },
                        { "date", 1700000026 },
                        { "peer_id", 200006334752 },
                        { "from_id", 200006334752 },
                        { "text", "/stat" },
                        { "random_id", 0 },
                        { "attachments", json::array() }
                    } }
              } }
        };
        Worker w;
        w.work(ev, fn, vk);
        const FakeNet::Call* msg = fn.firstExact("https://api.vk.ru/method/messages.send");
        check(msg && msg->params.count("peer_id") && msg->params.at("peer_id") == "200006334752",
              "U26a ответ ушёл на большой peer_id", msg ? (msg->params.count("peer_id") ? msg->params.at("peer_id") : "(нет peer_id)") : "(нет вызова)");
        bool inBd = false;
        bd.lock.lock();
        inBd = bd.get()["users"]["200006334752"].is_object();
        bd.lock.unlock();
        check(inBd, "U26b пользователь с большим id заведён");
    }

    // --- U27: ThreadPool (теперь часть проекта): future-результаты, post(), стойкость к исключениям ---
    {
        ThreadPool tp(2);
        std::atomic<int> sum{0};
        std::vector<std::future<void>> fs;
        for (int i = 1; i <= 50; ++i)
            fs.push_back(tp.enqueue([&sum, i] { sum += i; }));
        int r = tp.enqueue([](int x) { return x * 2; }, 21).get();
        for (auto& f : fs)
            f.get();
        check(sum == 1275 && r == 42, "U27a enqueue: результаты и future", std::to_string(sum) + "/" + std::to_string(r));
        bool caught = false;
        try
        {
            tp.enqueue([] { throw std::runtime_error("boom"); }).get();
        }
        catch (const std::exception&)
        {
            caught = true;
        }
        check(caught, "U27b исключение задачи доезжает до future");
        // post(): исключение не убивает поток пула (раньше — terminate процесса)
        tp.post([] { throw std::runtime_error("boom-post"); });
        bool failedSeen = false;
        for (int i = 0; i < 200 && !failedSeen; ++i)
        {
            timer::sleep(10);
            failedSeen = tp.failedTasks() >= 1;
        }
        int alive = tp.enqueue([] { return 7; }).get();
        check(failedSeen && alive == 7, "U27c пул жив после исключения в post", std::to_string(tp.failedTasks()));
    }

    // --- U28: fs::safeWriteData и BD::save — атомарная запись (убить процесс посреди save ≠ битый файл) ---
    {
        fs::safeWriteData("t_safe.json", "{\"a\":1}");
        check(fs::readData("t_safe.json") == "{\"a\":1}" && !fs::exists("t_safe.json.safe"),
              "U28a safeWriteData: содержимое на месте, .safe не остаётся");
        remove("t_safe.json");
        {
            BD b2("t_bd.json");
            b2.get()["k"] = "v";
            b2.save();
        }
        check(fs::readData("t_bd.json").find("\"k\"") != std::string::npos && !fs::exists("t_bd.json.safe"),
              "U28b BD::save пишет через .safe атомарно");
        remove("t_bd.json");
    }

    // --- U29: Vk::send на неразбираемый ответ API — null без исключения (найдено живой проверкой) ---
    {
        FakeNet fn;
        fn.defaultResponse = "service unavailable"; // не JSON
        Vk vk(fn);
        json r;
        bool threw = false;
        try
        {
            r = vk.send("users.get");
        }
        catch (...)
        {
            threw = true;
        }
        check(!threw && r.is_null(), "U29 битый ответ -> null, процесс жив", threw ? "исключение" : r.dump());
    }

    // --- U30: имена полей multipart-загрузки: «photo» для фото, «file» для документов ---
    {
        FakeNet fn;
        fn.responses["UploadServer"] = "{\"response\":{\"upload_url\":\"https://upload.example.com/us\"}}";
        fn.responses["upload.example.com"] = "{\"server\":937131,\"photo\":\"PH\",\"hash\":\"hh\",\"file\":\"FH\"}";
        fn.responses["saveMessagesPhoto"] = "{\"response\":[{\"id\":5,\"owner_id\":-1,\"date\":100,\"access_key\":\"ak\",\"sizes\":[{\"width\":50,\"height\":50,\"url\":\"http://x/y.jpg\"}]}]}";
        fn.responses["docs.save"] = "{\"response\":{\"type\":\"doc\",\"doc\":{\"id\":6,\"owner_id\":-1,\"access_key\":\"ak2\",\"ext\":\"txt\",\"date\":101}}}";
        Vk vk(fn);
        Doc ph;
        bool okPhoto = ph.uploadPhoto("n.jpg", "DATA", fn, vk, 123);
        const FakeNet::Call* upP = fn.first("upload.example.com");
        check(okPhoto && upP && upP->params.at("field") == "photo", "U30a фото — поле «photo»", upP ? upP->params.at("field") : "(нет вызова)");
        fn.reset();
        Doc dc;
        bool okDoc = dc.uploadDoc("n.txt", "DATA", fn, vk, 123);
        const FakeNet::Call* upD = fn.first("upload.example.com");
        check(okDoc && upD && upD->params.at("field") == "file", "U30b документ — поле «file»", upD ? upD->params.at("field") : "(нет вызова)");
    }

    // --- U31: новый APOD (science.nasa.gov/wp-json) и str::htmlToText ---
    {
        check(str::htmlToText("<strong>Привет</strong>, <a href=\"x\">мир</a> &amp; &#8217;космос&#8217;<br>тест") == "Привет, мир & ’космос’\nтест",
              "U31a htmlToText: теги/сущности/br", str::htmlToText("<strong>Привет</strong>, <a href=\"x\">мир</a> &amp; &#8217;космос&#8217;<br>тест"));
        FakeNet fn;
        fn.responses["users.get"] = "{\"response\":[{\"id\":444,\"first_name\":\"Гость\"}]}";
        fn.responses["/method/messages."] = "{\"response\":1}";
        fn.responses["apod-basic"] = "{\"date\":\"2026-10-24\",\"title\":\"Rosetta's Selfie\",\"media_type\":\"image\","
            "\"explanation\":\"<strong>Expl:</strong> привет &amp; &lt;мир&gt;\",\"credit\":\"ESA/Rosetta\","
            "\"hdurl\":\"https://assets.example.com/x.png\",\"url\":\"https://science.nasa.gov/apod/x/\",\"permalink\":\"https://science.nasa.gov/apod/x/\"}";
        fn.responses["assets.example.com"] = "IMGDATA";
        fn.responses["UploadServer"] = "{\"response\":{\"upload_url\":\"https://upload.example.com/us\"}}";
        fn.responses["upload.example.com"] = "{\"server\":937131,\"photo\":\"PH\",\"hash\":\"hh\"}";
        fn.responses["saveMessagesPhoto"] = "{\"response\":[{\"id\":5,\"owner_id\":-1,\"date\":100,\"access_key\":\"ak\",\"sizes\":[{\"width\":50,\"height\":50,\"url\":\"http://x/y.jpg\"}]}]}";
        Vk vk(fn);
        json ev = { { "type", "message_new" },
                    { "object", { { "date", 1 }, { "peer_id", 444 }, { "from_id", 444 }, { "text", "/apod" }, { "random_id", 1 } } } };
        Worker w;
        w.work(ev, fn, vk);
        const FakeNet::Call* msg = nullptr;
        for (const auto& x : fn.calls)
            if (x.url == "https://api.vk.ru/method/messages.send")
                msg = &x;
        std::string m = msg && msg->params.count("message") ? msg->params.at("message") : "(нет вызова)";
        bool attached = false;
        if (msg)
            for (const auto& p : msg->params)
                if (p.second.find("photo-1_5_ak") != std::string::npos) // формат VK: type+owner_id (без разделителя)
                    attached = true;
        check(msg && m.find("Rosetta's Selfie") != std::string::npos && m.find("Expl: привет & <мир>") != std::string::npos
              && m.find("<strong>") == std::string::npos, "U31b /apod новый формат, HTML вырезан", m.substr(0, 80));
        check(attached, "U31c /apod приложил фото (hdurl)", attached ? "есть" : "нет attachment");
        fn.reset();
        fn.responses["apod-basic"] = "<html><head><title>403 Forbidden</title></head></html>";
        Worker w2;
        w2.work(ev, fn, vk);
        msg = nullptr;
        for (const auto& x : fn.calls)
            if (x.url == "https://api.vk.ru/method/messages.send")
                msg = &x;
        m = msg && msg->params.count("message") ? msg->params.at("message") : "(нет вызова)";
        check(msg && m.find("APOD") != std::string::npos, "U31d /apod не-JSON -> вежливый ответ", m.substr(0, 80));
    }

    // --- U32: /apoddate и /apodrand (космос) ---
    {
        FakeNet fn;
        fn.responses["users.get"] = "{\"response\":[{\"id\":444,\"first_name\":\"Гость\",\"bdate\":\"24.10.2001\"}]}";
        fn.responses["/method/messages."] = "{\"response\":1}";
        fn.responses["apod-basic"] = "{\"date\":\"2014-10-01\",\"title\":\"Butterfly Nebula\",\"media_type\":\"video\",\"explanation\":\"тумба\",\"permalink\":\"https://science.nasa.gov/apod/x/\"}";
        Vk vk(fn);
        auto sendApod = [&](const std::string& text)
        {
            fn.reset();
            json ev = { { "type", "message_new" },
                        { "object", { { "date", 1 }, { "peer_id", 444 }, { "from_id", 444 }, { "text", text }, { "random_id", 1 } } } };
            Worker w;
            w.work(ev, fn, vk);
            const FakeNet::Call* msg = nullptr;
            for (const auto& x : fn.calls)
                if (x.url == "https://api.vk.ru/method/messages.send")
                    msg = &x;
            std::string apodUrl;
            for (const auto& x : fn.calls)
                if (x.url.find("apod-basic") != std::string::npos)
                    apodUrl = x.url;
            return std::make_pair(msg && msg->params.count("message") ? msg->params.at("message") : std::string("(нет вызова)"), apodUrl);
        };
        auto r1 = sendApod("/apoddate 01.10.2014");
        check(r1.first.find("Butterfly") != std::string::npos && r1.second.find("apod-basic/141001") != std::string::npos,
              "U32a /apoddate по дате из текста", r1.second + " | " + r1.first.substr(0, 40));
        auto r2 = sendApod("/apoddate"); // bdate 24.10.1990 из mock users.get
        check(r2.second.find("apod-basic/011024") != std::string::npos && r2.first.find("Butterfly") != std::string::npos,
              "U32b /apoddate по дню рождения профиля", r2.second + " | " + r2.first.substr(0, 40));
        fn.responses["users.get"] = "{\"response\":[{\"id\":444,\"first_name\":\"Гость\",\"bdate\":\"2.5.1970\"}]}"; // ДР до эры APOD
        auto r2b = sendApod("/apoddate");
        bool shifted = r2b.second.find("apod-basic/") != std::string::npos && r2b.first.find("Butterfly") != std::string::npos;
        check(shifted, "U32b2 доэрастное ДР — сдвиг на последний прошедший год", r2b.second + " | " + r2b.first.substr(0, 40));
        fn.responses["users.get"] = "{\"response\":[{\"id\":444,\"first_name\":\"Гость\"}]}"; // ДР скрыт
        auto r2c = sendApod("/apoddate");
        check(r2c.first.find("скрыта") != std::string::npos && r2c.second.empty(), "U32b3 скрытое ДР — просьба ввести дату", r2c.first.substr(0, 60));
        auto r3 = sendApod("/apoddate 29.02.2014");
        check(r3.first.find("не бывает") != std::string::npos && r3.second.empty(),
              "U32c /apoddate невалидная дата (29.02 невисокосный)", r3.first.substr(0, 60));
        auto r4 = sendApod("/apoddate 01.01.1990");
        check(r4.first.find("16.06.1995") != std::string::npos && r4.second.empty(),
              "U32d /apoddate до эры APOD", r4.first.substr(0, 60));
        auto r5 = sendApod("/apodrand");
        bool rndUrl = r5.second.find("apod-basic/") != std::string::npos && r5.second.size() == std::string("https://science.nasa.gov/wp-json/wp/v2/apod-basic/").size() + 6;
        check(rndUrl && r5.first.find("Butterfly") != std::string::npos, "U32e /apodrand — случайная дата YYMMDD", r5.second);
    }

    // --- U33: не-JSON ответ upload-сервера не роняет команду ---
    {
        FakeNet fn;
        fn.responses["UploadServer"] = "{\"response\":{\"upload_url\":\"https://upload.example.com/us\"}}";
        fn.responses["upload.example.com"] = "<html><head><title>413 Request Entity Too Large</title></head></html>";
        Vk vk(fn);
        Doc ph;
        bool threw = false;
        bool ok = true;
        try { ok = ph.uploadPhoto("big.jpg", "DATA", fn, vk, 123); }
        catch (...) { threw = true; }
        check(!threw && !ok, "U33a uploadPhoto на HTML-ответ отдаёт false, не бросает", threw ? "исключение" : (ok ? "true" : "false"));
        // полный конвейер: /apodrand, картинка не грузится -> сообщение со ссылкой на пост
        FakeNet fn2;
        fn2.responses["users.get"] = "{\"response\":[{\"id\":444,\"first_name\":\"Гость\"}]}";
        fn2.responses["/method/messages."] = "{\"response\":1}";
        fn2.responses["apod-basic"] = "{\"date\":\"1999-01-01\",\"title\":\"Huge One\",\"media_type\":\"image\",\"explanation\":\"огромная\",\"hdurl\":\"https://assets.example.com/huge.png\",\"permalink\":\"https://science.nasa.gov/apod/huge/\"}";
        fn2.responses["assets.example.com"] = "BIGIMG";
        fn2.responses["UploadServer"] = "{\"response\":{\"upload_url\":\"https://upload.example.com/us\"}}";
        fn2.responses["upload.example.com"] = "<html>413 Request Entity Too Large</html>";
        Vk vk2(fn2);
        json ev = { { "type", "message_new" },
                    { "object", { { "date", 1 }, { "peer_id", 444 }, { "from_id", 444 }, { "text", "/apodrand" }, { "random_id", 1 } } } };
        Worker w;
        w.work(ev, fn2, vk2);
        const FakeNet::Call* msg = nullptr;
        for (const auto& x : fn2.calls)
            if (x.url == "https://api.vk.ru/method/messages.send")
                msg = &x;
        std::string m = msg && msg->params.count("message") ? msg->params.at("message") : "(нет вызова)";
        check(msg && m.find("картинку загрузить не удалось") != std::string::npos && m.find("science.nasa.gov/apod/huge/") != std::string::npos,
              "U33b /apodrand с битой загрузкой фото — сообщение со ссылкой", m.substr(0, 70));
    }

    // --- U34: wall_reply_new (комментарий от имени другого сообщества) ---
    {
        FakeNet fn;
        fn.responses["wall.createComment"] = "{\"response\":900}";
        Vk vk(fn);
        json ev =
        {
            { "type", "wall_reply_new" },
            { "object", { { "id", 777 }, { "date", 5 }, { "text", "/stat" }, { "from_id", -555 },
                          { "post_id", 40579 }, { "owner_id", -178228638 } } }
        };
        Event e(fn, vk, ev);
        check(e.cid == 777 && e.id == 40579 && e.peer_id == -178228638 && e.from_id == -555,
              "U34a парсинг wall_reply_new (cid/id/peer_id)", std::to_string(e.cid) + "/" + std::to_string(e.id));
        Event out = e.getOut();
        check(out.cid == e.cid && out.type == e.type && out.id == e.id, "U34b getOut сохраняет cid/type");
        Worker w;
        w.work(ev, fn, vk);
        const FakeNet::Call* c = fn.first("wall.createComment");
        check(c && c->params.count("post_id") && c->params.at("post_id") == "40579"
                  && c->params.count("owner_id") && c->params.at("owner_id") == "-178228638"
                  && c->params.count("reply_to_cid") && c->params.at("reply_to_cid") == "777",
              "U34c ответ в ветку комментария (reply_to_cid)", c ? (c->params.count("reply_to_cid") ? c->params.at("reply_to_cid") : "(нет reply_to_cid)") : "(нет вызова)");
        check(c && c->params.count("message") && c->params.at("message").find("Работаю") != std::string::npos,
              "U34c2 команда выполнена (текст /stat)", c && c->params.count("message") ? c->params.at("message").substr(0, 40) : "(нет)");
        check(fn.first("users.get") == nullptr, "U34d users.get не зовётся для сообщества", fn.first("users.get") ? "(зовётся)" : "");
        check(bd.get()["users"]["-555"]["acess"] == 1, "U34e запись сообщества в bd с уровнем 1", bd.get()["users"]["-555"].dump());
    }

    // --- U35: комментарий самой страницы под своей записью — игнор (защита от петли) ---
    {
        FakeNet fn;
        fn.responses["wall.createComment"] = "{\"response\":901}";
        Vk vk(fn);
        json ev =
        {
            { "type", "wall_reply_new" },
            { "object", { { "id", 778 }, { "date", 5 }, { "text", "/stat" }, { "from_id", -178228638 },
                          { "post_id", 40579 }, { "owner_id", -178228638 } } }
        };
        Worker w;
        w.work(ev, fn, vk);
        check(fn.first("wall.createComment") == nullptr, "U35a свой комментарий проигнорирован", fn.first("wall.createComment") ? "(ответ!)" : "");
    }

    // --- U36: собственный комментарий с маркером «@@ » (режим самопроверки чекера) ---
    {
        FakeNet fn;
        fn.responses["wall.createComment"] = "{\"response\":902}";
        Vk vk(fn);
        json ev =
        {
            { "type", "wall_reply_new" },
            { "object", { { "id", 779 }, { "date", 5 }, { "text", "@@ /stat" }, { "from_id", -178228638 },
                          { "post_id", 40579 }, { "owner_id", -178228638 } } }
        };
        Worker w;
        w.work(ev, fn, vk);
        const FakeNet::Call* c = fn.first("wall.createComment");
        check(c && c->params.count("reply_to_cid") && c->params.at("reply_to_cid") == "779"
                  && c->params.count("message") && c->params.at("message").find("Работаю") != std::string::npos,
              "U36a «@@ /stat» от своей страницы выполнен", c && c->params.count("message") ? c->params.at("message").substr(0, 40) : "(нет вызова)");
        check(c && c->params.count("message") && c->params.at("message").find("@@") == std::string::npos,
              "U36b маркер из команды вырезан", c && c->params.count("message") ? c->params.at("message").substr(0, 30) : "");
    }

    // --- U37: отрицательный peer_id (wall_reply) при загрузке вложения ---
    {
        FakeNet fn;
        fn.responses["UploadServer"] = "{\"response\":{\"upload_url\":\"https://upload.example.com/us\"}}";
        fn.responses["upload.example.com"] = "{\"server\":1,\"photo\":\"PH\",\"hash\":\"hh\"}";
        fn.responses["saveMessagesPhoto"] = "{\"response\":[{\"id\":5,\"owner_id\":-178228638,\"date\":1,\"access_key\":\"ak\"}]}";
        Vk vk(fn);
        Doc ph;
        bool ok = ph.uploadPhoto("x.jpg", "DATA", fn, vk, -178228638);
        const FakeNet::Call* g = fn.first("getMessagesUploadServer");
        check(ok && g && g->params.count("peer_id") && g->params.at("peer_id") == "-178228638",
              "U37a peer_id знаковый, не переполнен в беззнаковый", g ? (g->params.count("peer_id") ? g->params.at("peer_id") : "(нет)") : "(нет вызова)");
    }

    // --- U38: пустой ответ не отправляется (VK запрещает пустые сообщения/комментарии) ---
    {
        FakeNet fn;
        fn.responses["wall.createComment"] = "{\"response\":1}";
        Vk vk(fn);
        Event e(fn, vk);
        e.type = "wall_reply_new";
        e.peer_id = -178228638; e.id = 40581; e.cid = 1;
        check(e.send().is_null() && fn.calls.empty(), "U38a пустой ответ не отправлен", std::to_string(fn.calls.size()));
        e.msg = "текст";
        check(!e.send().is_null() && !fn.calls.empty(), "U38b непустой ответ отправлен");
    }


    // --- U39: источник картинки арт-команд — вложение комментария или фото профиля ---
    {
        FakeNet fn;
        Vk vk(fn);
        json ev =
        {
            { "type", "wall_reply_new" },
            { "object", { { "id", 777 }, { "date", 5 }, { "text", "/neon" }, { "from_id", 987 },
                          { "post_id", 40581 }, { "owner_id", -178228638 },
                          { "attachment", { { "photo", { { "id", 3 }, { "owner_id", 987 }, { "date", 1 }, { "access_key", "ak" },
                                          { "sizes", json::array({ { { "width", 100 }, { "height", 80 }, { "url", "http://x/c.jpg" } } }) } } } } } } }
        };
        Event e(fn, vk, ev);
        check(e.docs.size() == 1 && e.docs[0].type == "photo" && e.docs[0].url == "http://x/c.jpg",
              "U39a одиночное вложение комментария (attachment)", e.docs.size() ? e.docs[0].url : "(нет доков)");
    }
    // U39b: /neon без вложения — берётся фото профиля автора
    {
        FakeNet fn;
        fn.responses["users.get"] = "{\"response\":[{\"id\":987,\"first_name\":\"Чел\",\"photo_200\":\"http://x/prof.jpg\"}]}";
static const unsigned char tinyPng[] = { 0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x04, 0x08, 0x02, 0x00, 0x00, 0x00, 0x26, 0x93, 0x09, 0x29, 0x00, 0x00, 0x00, 0x2e, 0x49, 0x44, 0x41, 0x54, 0x78, 0x9c, 0x0d, 0xc6, 0x31, 0x01, 0x00, 0x30, 0x0c, 0xc3, 0x30, 0x23, 0x29, 0x12, 0x23, 0x29, 0x92, 0x20, 0x19, 0x92, 0x00, 0x5c, 0x75, 0x09, 0x40, 0x26, 0x58, 0xf6, 0x3a, 0x6a, 0xdc, 0x1a, 0xc8, 0x75, 0x93, 0x34, 0x0f, 0xba, 0xf6, 0xfa, 0xda, 0x7e, 0x46, 0x83, 0x10, 0xe1, 0xb0, 0xad, 0x57, 0xc9, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82 };
        fn.responses["prof.jpg"] = std::string(reinterpret_cast<const char*>(tinyPng), sizeof(tinyPng));
        fn.responses["UploadServer"] = "{\"response\":{\"upload_url\":\"https://upload.example.com/us\"}}";
        fn.responses["upload.example.com"] = "{\"server\":1,\"photo\":\"PH\",\"hash\":\"hh\"}";
        fn.responses["saveMessagesPhoto"] = "{\"response\":[{\"id\":5,\"owner_id\":-178228638,\"date\":1,\"access_key\":\"ak\"}]}";
        fn.responses["wall.createComment"] = "{\"comment_id\":1}";
        Vk vk(fn);
        json ev = { { "type", "wall_reply_new" },
                    { "object", { { "id", 778 }, { "date", 5 }, { "text", "/neon" }, { "from_id", 987 },
                                  { "post_id", 40581 }, { "owner_id", -178228638 } } } };
        Worker w;
        w.work(ev, fn, vk);
        const FakeNet::Call* c = fn.first("wall.createComment");
        check(c && c->params.count("attachment") && c->params.at("attachment").find("photo") == 0,
              "U39b /neon без вложения обрабатывает фото профиля", c && c->params.count("attachment") ? c->params.at("attachment") : "(нет attachment)");
    }
    // U39c: нет ни вложения, ни фото — пустой ответ не отправляется
    {
        FakeNet fn;
        fn.responses["users.get"] = "{\"response\":[{\"id\":988,\"first_name\":\"Без фото\"}]}";
        fn.responses["wall.createComment"] = "{\"comment_id\":1}";
        Vk vk(fn);
        json ev = { { "type", "wall_reply_new" },
                    { "object", { { "id", 779 }, { "date", 5 }, { "text", "/neon" }, { "from_id", 988 },
                                  { "post_id", 40581 }, { "owner_id", -178228638 } } } };
        Worker w;
        w.work(ev, fn, vk);
        check(fn.first("wall.createComment") == nullptr, "U39c без картинки — комментарий не отправлен", fn.first("wall.createComment") ? "(отправлен!)" : "");
    }
    // U39d: загрузка фото не удалась — объяснение вместо молчания
    {
        FakeNet fn;
        static const unsigned char tinyPng2[] = { 0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x04, 0x08, 0x02, 0x00, 0x00, 0x00, 0x26, 0x93, 0x09, 0x29, 0x00, 0x00, 0x00, 0x2e, 0x49, 0x44, 0x41, 0x54, 0x78, 0x9c, 0x0d, 0xc6, 0x31, 0x01, 0x00, 0x30, 0x0c, 0xc3, 0x30, 0x23, 0x29, 0x12, 0x23, 0x29, 0x92, 0x20, 0x19, 0x92, 0x00, 0x5c, 0x75, 0x09, 0x40, 0x26, 0x58, 0xf6, 0x3a, 0x6a, 0xdc, 0x1a, 0xc8, 0x75, 0x93, 0x34, 0x0f, 0xba, 0xf6, 0xfa, 0xda, 0x7e, 0x46, 0x83, 0x10, 0xe1, 0xb0, 0xad, 0x57, 0xc9, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82 };
        fn.responses["users.get"] = "{\"response\":[{\"id\":989,\"first_name\":\"Чел2\",\"photo_200\":\"http://x/prof.jpg\"}]}";
        fn.responses["prof.jpg"] = std::string(reinterpret_cast<const char*>(tinyPng2), sizeof(tinyPng2));
        fn.responses["wall.createComment"] = "{\"comment_id\":1}";
        Vk vk(fn);
        json ev = { { "type", "wall_reply_new" },
                    { "object", { { "id", 780 }, { "date", 5 }, { "text", "/neon" }, { "from_id", 989 },
                                  { "post_id", 40581 }, { "owner_id", -178228638 } } } };
        Worker w;
        w.work(ev, fn, vk);
        const FakeNet::Call* c = fn.first("wall.createComment");
        std::string m = c && c->params.count("message") ? c->params.at("message") : "(нет вызова)";
        check(m.find("приложить не удалось") != std::string::npos, "U39d сбой загрузки — есть объяснение", m.substr(0, 80));
    }

    // --- U40: загрузка фото — пустой photo и новый формат ответа upload-сервера (2026) ---
    {
        FakeNet fn;
        fn.responses["UploadServer"] = "{\"response\":{\"upload_url\":\"https://upload.example.com/us\"}}";
        fn.responses["upload.example.com"] = "{\"server\":1,\"photo\":\"\",\"hash\":\"hh\"}";
        fn.responses["saveMessagesPhoto"] = "{\"response\":[{\"id\":5,\"owner_id\":-1,\"date\":1,\"access_key\":\"ak\"}]}";
        Vk vk(fn);
        Doc ph;
        bool ok = ph.uploadPhoto("x.jpg", "DATA", fn, vk, 123);
        int saves = 0, ups = 0;
        for (const auto& c : fn.calls)
        {
            if (c.url.find("saveMessagesPhoto") != std::string::npos)
                saves++;
            if (c.url == "https://upload.example.com/us")
                ups++;
        }
        check(!ok && saves == 0, "U40a пустой photo — false, saveMessagesPhoto не зовётся", std::to_string(saves));
        check(ups == 2, "U40b повтор попытки загрузки при пустом photo", std::to_string(ups));
    }
    {
        FakeNet fn;
        fn.responses["UploadServer"] = "{\"response\":{\"upload_url\":\"https://upload.example.com/us\"}}";
        fn.responses["upload.example.com"] = "{\"server\":1,\"photo\":\"[{\\\"photo\\\":\\\"abc:x\\\"}]\",\"hash\":\"hh\"}";
        fn.responses["saveMessagesPhoto"] = "{\"response\":[{\"id\":6,\"owner_id\":-1,\"date\":1,\"access_key\":\"ak2\",\"sizes\":[{\"width\":10,\"height\":10,\"url\":\"http://x/p.jpg\"}]}]}";
        Vk vk(fn);
        Doc ph;
        bool ok = ph.uploadPhoto("x.jpg", "DATA", fn, vk, 123);
        const FakeNet::Call* sv = fn.first("saveMessagesPhoto");
        check(ok && sv && sv->params.count("photo") && sv->params.at("photo") == "[{\"photo\":\"abc:x\"}]",
              "U40c новый формат photo (JSON-строка) пропускается как есть", sv ? sv->params.count("photo") ? sv->params.at("photo") : "(нет)" : "(нет вызова)");
    }

    Net::deinit();

    std::cout << "\n=== " << passed << " passed, " << failed << " failed ===" << std::endl;
    return failed ? 1 : 0;
}
