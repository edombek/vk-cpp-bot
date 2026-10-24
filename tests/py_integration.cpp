// Автоматический интеграционный тест python-слоя (фаза 2).
// Запускается через ctest. Не требует сети и VK-токенов: проверяются pyF
// (под-интерпретатор на скрипт), изоляция, обработка ошибок, кастеры типов
// (dict<->table_t, list, json), биндинги подмодулей (fs/str), read-only поля
// и — главное — многопоточная схема GIL (под-интерпретатор из нескольких потоков),
// которая без PyMainThread(gil_scoped_release) уходит в deadlock.
#include "py.h"

#include "bd.h"
#include "cmd.h"
#include "events.h"
#include "fakenet.h"
#include "fs.h"
#include "img.h"
#include "net.h"
#include "str.h"
#include "users.h"
#include "vk.h"

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
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

        // --- T17: метаданные python-аддонов (pyAddonMeta): чтение и валидация ---
        {
            PyAddonMeta m = pyAddonMeta(
                "cmd = '/t17'\ninfo = 'тест'\nacess = 2\ncat = 'python'\n"
                "def run(eventIn, eventOut, net, vk):\n    return 'ok'\n");
            check(m.ok(), "T17a meta ok", m.error);
            check(m.cmd == "/t17" && m.info == "тест" && m.acess == 2 && m.cat == "python",
                  "T17b meta fields", m.cmd + "/" + std::to_string(m.acess) + "/" + m.cat);
            check(!pyAddonMeta("cmd = '/x'\ninfo='i'\nacess=1\ncat='c'").ok(), "T17c нет run()");
            check(!pyAddonMeta("cmd = '/x'\ninfo='i'\nacess=9\ncat='c'\ndef run(*a): pass").ok(), "T17d acess вне 1..5");
            check(!pyAddonMeta("cmd = 'x'\ninfo='i'\nacess=1\ncat='c'\ndef run(*a): pass").ok(), "T17e cmd без «/»");
            check(!pyAddonMeta("def (").ok(), "T17f синтаксическая ошибка");
            check(!pyAddonMeta("cmd = '/x y'\ninfo='i'\nacess=1\ncat='c'\ndef run(*a): pass").ok(), "T17g cmd с пробелом");
        }

        // --- T18: запуск аддона (pyF::execAddon): msg + возврат run() + print; ошибки ---
        {
            FakeNet fn;
            fn.responses["/method/messages.send"] = "{\"response\":1}";
            Vk vkp(fn);
            Event in(fn, vkp), out(fn, vkp);
            pyF f(in, out);
            std::string err = f.execAddon(
                "cmd='/t18'\ninfo='i'\nacess=1\ncat='python'\n"
                "def run(eventIn, eventOut, net, vk):\n"
                "    eventOut.msg = 'база'\n"
                "    print('печать')\n"
                "    return '-хвост'\n");
            check(err.empty(), "T18a аддон выполнен", err);
            check(out.msg == "база-хвост", "T18b msg + возврат run()", out.msg);
            check(fn.find("/method/messages.send").size() == 1, "T18c print из аддона");
            Event in2(fn, vkp), out2(fn, vkp);
            pyF f2(in2, out2);
            std::string err2 = f2.execAddon(
                "cmd='/t'\ninfo='i'\nacess=1\ncat='c'\n"
                "def run(eventIn, eventOut, net, vk):\n    raise ValueError('сбой')\n");
            check(!err2.empty() && err2.find("ValueError") != std::string::npos, "T18d traceback из run()", err2);
            Event in3(fn, vkp), out3(fn, vkp);
            pyF f3(in3, out3);
            std::string err3 = f3.execAddon("cmd='/t'\ninfo='i'\nacess=1\ncat='c'\n");
            check(!err3.empty() && err3.find("run") != std::string::npos, "T18e нет run() при запуске", err3);
        }

        // --- T19: cmd::loadAddons + диспетчер: рабочий, битый и «заслон built-in» ---
        {
            namespace fsx = std::filesystem;
            std::string dir = "t_addons";
            fsx::create_directories(dir);
            auto writeFile = [](const std::string& p, const std::string& s)
            {
                std::ofstream f(p);
                f << s;
            };
            writeFile(dir + "/good.py",
                      "cmd = '/t19echo'\ninfo = 'эхо'\nacess = 1\ncat = 'pythonaddon'\n"
                      "def run(eventIn, eventOut, net, vk):\n    return 'эхо: ' + eventIn.msg\n");
            writeFile(dir + "/broken.py",
                      "cmd = '/t19broken'\ninfo = 'сломан'\nacess = 1\ncat = 'pythonaddon'\nесли ( синтаксис");
            writeFile(dir + "/shadow.py",
                      "cmd = '/stat'\ninfo = 'заслон'\nacess = 1\ncat = 'pythonaddon'\n"
                      "def run(eventIn, eventOut, net, vk):\n    return 'нет'\n");
            cmd::loadAddons(dir);
            auto list = cmd::addonList();
            check(list.size() == 1 && list[0].find("/t19echo") != std::string::npos,
                  "T19a загружен только валидный", std::to_string(list.size()));
            FakeNet fn;
            Vk vkp(fn);
            Event in(fn, vkp), out(fn, vkp);
            in.user = out.user = users::user { "U", 1, 1, 0 };
            in.msg = "привет";
            check(cmd::start("/t19echo", in, out, nullptr), "T19b аддон диспетчеризуется");
            check(out.msg.find("эхо: привет") != std::string::npos, "T19c ответ аддона", out.msg);
            check(!cmd::start("/t19broken", in, out, nullptr), "T19d битый не зарегистрирован");
            out.msg.clear();
            check(cmd::start("/help", in, out, nullptr) && out.msg.find("pythonaddon") != std::string::npos,
                  "T19e новый раздел в /help", out.msg.substr(0, 120));
            out.msg.clear();
            check(cmd::start("/pythonaddon", in, out, nullptr) && out.msg.find("pythonaddon") != std::string::npos,
                  "T19e2 ярлык «/pythonaddon» == /help pythonaddon", out.msg.substr(0, 120));
            // перечитывание (аналог /addonreload): правка файла применяется, built-in не заслоняется
            writeFile(dir + "/good.py",
                      "cmd = '/t19echo'\ninfo = 'эхо2'\nacess = 1\ncat = 'pythonaddon'\n"
                      "def run(eventIn, eventOut, net, vk):\n    return 'новый'\n");
            cmd::loadAddons(dir);
            out.msg.clear();
            check(cmd::start("/t19echo", in, out, nullptr) && out.msg.find("новый") != std::string::npos,
                  "T19f reload применяет правку", out.msg);
            fsx::remove(dir + "/good.py"); // пустой каталог -> снимает регистрацию аддонов
            cmd::loadAddons(dir);
            check(cmd::addonList().empty(), "T19g reload снимает старые команды");
            fsx::remove_all(dir);
        }

        // --- T20: Net.send авто-типизация: текст -> str, бинарный ответ -> bytes ---
        {
            FakeNet fn;
            std::string bin = std::string("\x89PNG\r\n\x1a\n", 8) + std::string("\xff\xfe\x00рядом", 9); // не UTF-8
            fn.responses["text.example"] = "{\"a\": 1}";
            fn.responses["bin.example"] = bin;
            Vk vkp(fn);
            RunResult r = run(fn, vkp,
                              "a = net.send('https://text.example/')\n"
                              "b = net.send('https://bin.example/')\n"
                              "eventOut.msg = type(a).__name__ + '/' + type(b).__name__ + '/' + str(len(b))");
            check(r.err.empty(), "T20a send str/bytes exec", r.err);
            check(r.msg == "str/bytes/" + std::to_string(bin.size()), "T20b автоопределение bytes", r.msg);
        }

        // --- T21: vkcppbot.img: fromBytes/invert/gray/scale/toPng, uploadPhoto (bytes, peer<0),
        // sources+fromDoc (фото профиля через FakeNet), readData бинарных файлов ---
        {
            cv::Mat src = cv::Mat::zeros(8, 6, CV_8UC3); // width 6, height 8
            src.at<cv::Vec3b>(2, 3) = cv::Vec3b(10, 200, 30);
            std::vector<uchar> buf;
            cv::imencode(".png", src, buf, {});
            fs::writeData("t_img.png", std::string(buf.begin(), buf.end()));
            FakeNet fn;
            fn.responses["UploadServer"] = "{\"response\":{\"upload_url\":\"https://upload.example.com/us\"}}";
            fn.responses["upload.example.com"] = "{\"server\":1,\"photo\":\"PH\",\"hash\":\"hh\"}";
            fn.responses["saveMessagesPhoto"] = "{\"response\":[{\"id\":5,\"owner_id\":-1,\"date\":100,\"access_key\":\"ak\",\"sizes\":[{\"width\":50,\"height\":50,\"url\":\"http://x/y.jpg\"}]}]}";
            fn.responses["users.get"] = "{\"response\":[{\"id\":1,\"photo_200\":\"http://p/200.png\"}]}";
            fn.responses["p/200.png"] = std::string(buf.begin(), buf.end());
            Vk vkp(fn);
            Event in(fn, vkp), out(fn, vkp);
            in.from_id = 1; // sources без вложений берёт фото профиля автора
            pyF f(in, out);
            std::string script =
                "import vkcppbot\n"
                "data = vkcppbot.fs.readData('t_img.png')\n"
                "im = vkcppbot.img.fromBytes(data)\n"
                "eventOut.msg = str(im.width) + 'x' + str(im.height) + '/' + str(len(im.toPng()) > 0)\n"
                "g = im.gray().scale(3, 4)\n"
                "eventOut.msg += '/' + str(g.width) + 'x' + str(g.height)\n"
                "docs = vkcppbot.img.sources(eventIn)\n"
                "im2 = vkcppbot.img.fromDoc(docs[0], net)\n"
                "eventOut.msg += '/src-' + str(im2.width)\n"
                "doc = vkcppbot.Doc()\n"
                "if doc.uploadPhoto('img.png', im.invert().toPng(), net, vk, -1):\n"
                "    eventOut.msg += '/up-' + doc.get()\n";
            std::string err = f.exec(script);
            check(err.empty(), "T21a image api exec", err);
            check(out.msg == "6x8/True/3x4/src-6/up-photo-1_5_ak", "T21b image api result", out.msg);
            const FakeNet::Call* up = fn.first("upload.example.com/us");
            check(up && up->body.rfind("\x89PNG", 0) == 0, "T21c png ушла в upload", up ? std::to_string(up->body.size()) : "нет upload");
            remove("t_img.png");
        }
    }

    remove("test_conf.json");
    remove("test_dat.json");
    remove("t_pyio.txt");

    Net::deinit();

    std::cout << "\n=== " << passed << " passed, " << failed << " failed ===" << std::endl;
    return failed ? 1 : 0;
}
