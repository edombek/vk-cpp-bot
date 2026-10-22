// Автоматический интеграционный тест python-слоя (фаза 2).
// Запускается через ctest. Не требует сети и VK-токенов: проверяются pyF
// (под-интерпретатор на скрипт), изоляция, обработка ошибок, кастеры типов
// (dict<->table_t, list, json), биндинги подмодулей (fs/str), read-only поля
// и — главное — многопоточная схема GIL (под-интерпретатор из нескольких потоков),
// которая без PyMainThread(gil_scoped_release) уходит в deadlock.
#include "py.h"

#include "bd.h"
#include "events.h"
#include "fs.h"
#include "net.h"
#include "str.h"
#include "users.h"
#include "vk.h"

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
    }

    remove("test_conf.json");
    remove("test_dat.json");
    remove("t_pyio.txt");

    Net::deinit();

    std::cout << "\n=== " << passed << " passed, " << failed << " failed ===" << std::endl;
    return failed ? 1 : 0;
}
