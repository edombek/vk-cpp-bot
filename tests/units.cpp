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
              msg->params.at("message").find("игры (") != std::string::npos &&
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
        check(e.peer_id == static_cast<uint32_t>(-1) && e.id == 3, "U14a wall ids");
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

    // --- U20: /w без аргумента — кнопки городов; /w Москва — разбор погоды (FakeNet) ---
    {
        conf.lock.lock();
        conf.get()["openweathermap_token"] = "TESTKEY";
        conf.lock.unlock();
        FakeNet fn;
        fn.responses["users.get"] = "{\"response\":[{\"id\":444,\"first_name\":\"Город\"}]}";
        fn.responses["api.openweathermap.org"] =
            "{\"main\":{\"temp\":20.5,\"humidity\":50},\"wind\":{\"speed\":3},"
            "\"weather\":[{\"description\":\"ясно\"}],\"name\":\"Moscow\",\"sys\":{\"country\":\"RU\"}}";
        fn.responses["/method/messages."] = "{\"response\":1}"; // ключ с /method/ — иначе матчит и .../data/2.5/weather
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
        check(r2.first.find("погода в RU/Moscow") != std::string::npos, "U20c weather parsed", r2.first.substr(0, 80));
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

    Net::deinit();

    std::cout << "\n=== " << passed << " passed, " << failed << " failed ===" << std::endl;
    return failed ? 1 : 0;
}
