// Автоматический интеграционный тест python-слоя (фаза 2).
// Запускается через ctest. Не требует сети и VK-токенов: проверяются pyF
// (под-интерпретатор на скрипт), изоляция, обработка ошибок, кастеры типов
// (dict<->table_t, list, json), биндинги подмодулей (fs/str), read-only поля
// и — главное — многопоточная схема GIL (под-интерпретатор из нескольких потоков),
// которая без PyMainThread(gil_scoped_release) уходит в deadlock.
#include "py.h"

#include "bd.h"
#include "events.h"
#include "fakenet.h"
#include "fs.h"
#include "net.h"
#include "str.h"
#include "users.h"
#include "vk.h"

#include <chrono>
#include <cstdio>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

// глобалы, обычно определяемые main.cpp
BD conf("test_conf.json");
BD bd("test_dat.json");

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

struct RunResult
{
    std::string err; // "" == успех
    std::string msg; // eventOut.msg после выполнения
    std::string keyboard; // eventOut.keyboard после выполнения
};

// запуск кода в свежем pyF (свежий под-интерпретатор + своё пространство имён)
static RunResult run(Net& net, Vk& vk, const std::string& code)
{
    Event in(net, vk), o(net, vk);
    pyF f(in, o);
    std::string c = code;
    RunResult r;
    r.err = f.exec(c);
    r.msg = o.msg;
    r.keyboard = o.keyboard;
    return r;
}

int main()
{
    Net::init();
    {
        // PyMainThread инициализирует интерпретатор и отпускает GIL главного потока.
        PyMainThread pymain;

        Net net;
        Vk vk(net); // конструктор сеть не трогает (валидация отдельно в Lp)

        // --- T1: скрипт меняет eventOut.msg, работает модуль vkcppbot.str ---
        {
            RunResult r = run(net, vk,
                              "import vkcppbot\n"
                              "eventOut.msg = 'hi-' + vkcppbot.str.low('WORLD')");
            check(r.err.empty(), "T1 exec ok", r.err);
            check(r.msg == "hi-world", "T1 result", r.msg);
        }

        // --- T2: ошибка времени исполнения возвращает traceback, процесс жив ---
        {
            RunResult r = run(net, vk, "1/0");
            check(!r.err.empty() && r.err.find("ZeroDivisionError") != std::string::npos,
                  "T2 runtime error caught", r.err);
        }

        // --- T3: синтаксическая ошибка тоже возвращается как текст ---
        {
            RunResult r = run(net, vk, "eventOut.msg = (");
            check(!r.err.empty() && r.err.find("SyntaxError") != std::string::npos,
                  "T3 syntax error caught", r.err);
        }

        // --- T4: изоляция под-интерпретаторов (глобалы не текут между /py) ---
        {
            RunResult a = run(net, vk, "секрет = 42");
            check(a.err.empty(), "T4a first run", a.err);
            RunResult b = run(net, vk, "eventOut.msg = str(секрет)");
            check(!b.err.empty() && b.err.find("NameError") != std::string::npos,
                  "T4b isolated interpreter", b.err);
        }

        // --- T5: подмодуль vkcppbot.fs (кастеры str<->C++, unicode) ---
        {
            RunResult r = run(net, vk,
                              "import vkcppbot\n"
                              "vkcppbot.fs.writeData('t_pyio.txt', 'abc\\u0430')\n"
                              "eventOut.msg = vkcppbot.fs.readData('t_pyio.txt')");
            check(r.err.empty(), "T5a fs roundtrip exec", r.err);
            check(r.msg == "abcа", "T5b fs roundtrip data", r.msg); // 'abcа' с кириллической 'а'
            remove("t_pyio.txt");
        }

        // --- T6: кастер std::vector<std::string> <-> list (str.words + str.summ) ---
        {
            RunResult r = run(net, vk,
                              "import vkcppbot\n"
                              "w = vkcppbot.str.words('x y z')\n"
                              "eventOut.msg = vkcppbot.str.summ(w, 1)");
            check(r.err.empty(), "T6a list caster", r.err);
            check(r.msg == "y z", "T6b list caster result", r.msg);
        }

        // --- T7: биндинг Doc (свойstva + get()) ---
        {
            RunResult r = run(net, vk,
                              "import vkcppbot\n"
                              "d = vkcppbot.Doc()\n"
                              "d.type = 'photo'\nd.owner_id = -1\nd.doc_id = 5\n"
                              "eventOut.msg = d.get()");
            check(r.err.empty(), "T7a doc binding", r.err);
            check(r.msg == "photo-1_5", "T7b doc.get()", r.msg);
        }

        // --- T8: eventIn доступен скрипту; read-only поля защищены ---
        {
            Event in(net, vk), out(net, vk);
            in.msg = "привет мир";
            in.words = str::words(in.msg);
            in.peer_id = 777;
            {
                pyF f(in, out);
                std::string c = "eventOut.msg = eventIn.words[1] + '/' + str(eventIn.peer_id)";
                std::string err = f.exec(c);
                check(err.empty(), "T8a read eventIn", err);
                check(out.msg == "мир/777", "T8b eventIn data", out.msg);
            }
            out.msg.clear();
            {
                pyF f(in, out);
                std::string c = "eventIn.peer_id = 1"; // read-only -> AttributeError
                std::string err = f.exec(c);
                check(!err.empty() && err.find("AttributeError") != std::string::npos,
                      "T8c readonly enforced", err);
            }
        }

        // --- T9: ГIL/многопоточность — N потоков крутят свои pyF одновременно.
        // Регрессия на deadlock: без gil_scoped_release в PyMainThread это виснет. ---
        {
            const int N = 8;
            std::vector<std::thread> th;
            std::vector<int> ok(N, 0);
            for (int i = 0; i < N; i++)
                th.emplace_back([&, i]()
                                {
                                    RunResult r = run(net, vk,
                                                      "import vkcppbot\n"
                                                      "eventOut.msg = vkcppbot.str.low('OK')");
                                    ok[i] = (r.err.empty() && r.msg == "ok") ? 1 : 0;
                                });
            for (auto& t : th)
                t.join();
            int good = 0;
            for (int i = 0; i < N; i++)
                good += ok[i];
            check(good == N, "T9 concurrent subinterpreters", std::to_string(good) + "/" + std::to_string(N));
        }

        // --- T10: Keyboard — построение VK-клавиатуры из скрипта /py ---
        {
            RunResult r = run(net, vk,
                              "import vkcppbot\n"
                              "k = vkcppbot.Keyboard(False, False)\n"
                              "k.button('да', '/yes', 'positive').row().textButton('нет')\n"
                              "eventOut.keyboard = k.dump()");
            check(r.err.empty(), "T10a keyboard exec", r.err);
            bool ok = false;
            try
            {
                json j = json::parse(r.keyboard);
                // payload VK-формата: JSON-строка {"cmd": "..."}
                ok = j["buttons"][0][0]["action"]["payload"].is_string() &&
                     json::parse(j["buttons"][0][0]["action"]["payload"].get<std::string>())["cmd"] == "/yes" &&
                     j["buttons"][0][0]["color"] == "positive" && j["buttons"][1][0]["action"]["type"] == "text";
            }
            catch (...)
            {
            }
            check(ok, "T10b keyboard json", r.keyboard);
        }

        // --- T11: print() отправляет отдельные сообщения (в тестах без пауз) ---
        {
            conf.lock.lock();
            conf.get()["py_print_interval_ms"] = 0; // без пауз внутри теста
            conf.get()["py_print_max_msgs"] = 30;
            conf.lock.unlock();
            FakeNet fn;
            fn.responses["/method/messages.send"] = "{\"response\":1}";
            Vk vkp(fn);
            RunResult r = run(fn, vkp, "print('привет')\nprint(2 + 2)");
            auto sends = fn.find("/method/messages.send");
            check(r.err.empty() && sends.size() == 2, "T11a два сообщения из print", std::to_string(sends.size()) + " " + r.err);
            bool content = sends.size() == 2 && sends[0]->params.at("message") == "привет" && sends[1]->params.at("message") == "4";
            check(content, "T11b содержимое print", sends.empty() ? "" : sends[0]->params.at("message"));
        }

        // --- T12: eventOut.msg и print — msg отправляет вызывающий код, print — отдельно ---
        {
            FakeNet fn;
            fn.responses["/method/messages.send"] = "{\"response\":1}";
            Vk vkp(fn);
            RunResult r = run(fn, vkp, "eventOut.msg = 'шаг'\nprint('ещё')");
            auto sends = fn.find("/method/messages.send");
            bool ok = r.msg == "шаг" && sends.size() == 1 && sends[0]->params.at("message") == "ещё";
            check(ok, "T12 msg и print раздельно", r.msg + " / " + std::to_string(sends.size()));
        }

        // --- T13: print до исключения отправлен, ошибка возвращена как раньше ---
        {
            FakeNet fn;
            fn.responses["/method/messages.send"] = "{\"response\":1}";
            Vk vkp(fn);
            RunResult r = run(fn, vkp, "print('до')\n1 / 0");
            auto sends = fn.find("/method/messages.send");
            check(!r.err.empty() && r.err.find("ZeroDivisionError") != std::string::npos, "T13a ошибка вернулась", r.err);
            bool ok = sends.size() == 1 && sends[0]->params.at("message") == "до";
            check(ok, "T13b print до ошибки", std::to_string(sends.size()));
        }

        // --- T14: флуд-контроль — максимум print-сообщений на скрипт, потом уведомление ---
        {
            conf.lock.lock();
            conf.get()["py_print_max_msgs"] = 3;
            conf.lock.unlock();
            FakeNet fn;
            fn.responses["/method/messages.send"] = "{\"response\":1}";
            Vk vkp(fn);
            RunResult r = run(fn, vkp, "for i in range(10): print(i)");
            auto sends = fn.find("/method/messages.send");
            bool ok = sends.size() == 4 && // 3 печати + уведомление об обрезке
                      sends.back()->params.at("message").find("обрезан") != std::string::npos;
            check(ok, "T14 лимит print-сообщений", std::to_string(sends.size()));
            conf.lock.lock();
            conf.get()["py_print_max_msgs"] = 30;
            conf.lock.unlock();
        }

        // --- T15: флуд-контроль — минимальный интервал между print-сообщениями ---
        {
            conf.lock.lock();
            conf.get()["py_print_interval_ms"] = 300;
            conf.lock.unlock();
            FakeNet fn;
            fn.responses["/method/messages.send"] = "{\"response\":1}";
            Vk vkp(fn);
            auto t0 = std::chrono::steady_clock::now();
            RunResult r = run(fn, vkp, "print('a')\nprint('b')\nprint('c')");
            long long ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
            auto sends = fn.find("/method/messages.send");
            check(sends.size() == 3 && ms >= 590, "T15 пауза между print", std::to_string(ms) + "ms / " + std::to_string(sends.size()));
            conf.lock.lock();
            conf.get()["py_print_interval_ms"] = 0;
            conf.lock.unlock();
        }

        // --- T16: /pywiki — текст вики исполняется как скрипт и уходит print-сообщениями ---
        {
            FakeNet fn;
            fn.responses["/method/messages.send"] = "{\"response\":1}";
            Vk vkp(fn);
            RunResult r = run(fn, vkp, pyWikiScript());
            auto sends = fn.find("/method/messages.send");
            bool content = sends.size() >= 5 && !sends.empty() &&
                           sends.front()->params.count("message") &&
                           sends.front()->params.at("message").find("Wiki /py") != std::string::npos;
            check(r.err.empty(), "T16a вики-скрипт без ошибок", r.err);
            check(content, "T16b вики печатается разделами", std::to_string(sends.size()));
        }
    }

    remove("test_conf.json");
    remove("test_dat.json");
    remove("t_pyio.txt");

    Net::deinit();

    std::cout << "\n=== " << passed << " passed, " << failed << " failed ===" << std::endl;
    return failed ? 1 : 0;
}
