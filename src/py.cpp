#include "py.h"

#include <chrono>
#include <memory>
#include <string>

#include <pybind11/pybind11.h>
#include <pybind11/embed.h>
#include <pybind11/eval.h>
#include <pybind11/gil.h>
#include <pybind11/subinterpreter.h>

#include "common.h"
#include "py_casters.h"

#include "doc.h"
#include "fs.h"
#include "img.h"
#include "keyboard.h"
#include "net.h"
#include "str.h"
#include "timer.h"
#include "users.h"
#include "vk.h"

namespace py = pybind11;

// ответ HTTP/файла в python: текст — как str; бинарные данные (картинки) — py::bytes.
// pybind при привязке std::string бросает UnicodeDecodeError на не-UTF-8 —
// на него и ориентируемся
static py::object pyTextOrBytes(const std::string& s)
{
    try
    {
        return py::str(s);
    }
    catch (const py::error_already_set&)
    {
        return py::bytes(s);
    }
}

// обёртка cv::Mat для python (фаза 12): OpenCV-типы в скрипты не просачиваются,
// только данные и пиксельные операции
struct PyImg
{
    cv::Mat mat;
    static PyImg decode(const std::string& data)
    {
        std::vector<uchar> v(data.begin(), data.end());
        PyImg im{cv::imdecode(v, 1)};
        if (im.mat.empty())
            throw std::invalid_argument("не удалось декодировать изображение (не картинка?)");
        return im;
    }
    std::string encode(const std::string& ext, int quality = -1) const
    {
        if (mat.empty())
            throw std::invalid_argument("пустое изображение");
        std::vector<uchar> buff;
        std::vector<int> params;
        if (quality >= 0)
            params = {cv::IMWRITE_JPEG_QUALITY, quality};
        if (!cv::imencode(ext, mat, buff, params))
            throw std::invalid_argument("не удалось закодировать в " + ext);
        return std::string(buff.begin(), buff.end());
    }
};

// ---------------------------------------------------------------------------
// Модуль `vkcppbot`: embedded (регистрируется через PyImport_AppendInittab на
// статической инициализации до main), импортируется в КАЖДОМ под-интерпретаторе
// заново — типы создаются на интерпретатор. per_interpreter_gil обязателен для
// многопоточности (см. docs/TESTS.md).
// ---------------------------------------------------------------------------
PYBIND11_EMBEDDED_MODULE(vkcppbot, m, py::multiple_interpreters::per_interpreter_gil())
{
    m.attr("__version__") = GIT_VER;

    py::class_<users::user>(m, "User")
        .def_readwrite("name", &users::user::name)
        .def_readwrite("id", &users::user::id)
        .def_readwrite("acess", &users::user::acess)
        .def_readwrite("msgs", &users::user::msgs);

    py::class_<Doc>(m, "Doc")
        .def(py::init<>())
        .def_readwrite("type", &Doc::type)
        .def_readwrite("owner_id", &Doc::owner_id)
        .def_readwrite("doc_id", &Doc::doc_id)
        .def_readwrite("acess_key", &Doc::acess_key)
        .def_readwrite("ext", &Doc::ext)
        .def_readwrite("url", &Doc::url)
        .def_readwrite("timestamp", &Doc::timestamp)
        .def("get", [](const Doc& self) { return pyTextOrBytes(self.get()); })
        .def("uploadDoc",
             [](Doc& self, std::string filename, py::object data, Net& net, Vk& vk, int64_t peer_id, bool audio)
             {
                 std::string d = py::isinstance<py::bytes>(data) ? py::cast<std::string>(data) : std::string(py::str(data));
                 return self.uploadDoc(filename, d, net, vk, peer_id, audio);
             },
             py::arg("filename"), py::arg("data"), py::arg("net"), py::arg("vk"), py::arg("peer_id"), py::arg("audio_message") = false)
        .def("uploadPhoto",
             [](Doc& self, std::string filename, py::object data, Net& net, Vk& vk, int64_t peer_id)
             {
                 std::string d = py::isinstance<py::bytes>(data) ? py::cast<std::string>(data) : std::string(py::str(data));
                 return self.uploadPhoto(filename, d, net, vk, peer_id);
             },
             py::arg("filename"), py::arg("data"), py::arg("net"), py::arg("vk"), py::arg("peer_id"));

    py::class_<Event>(m, "Event")
        .def_readwrite("msg", &Event::msg)
        .def_readwrite("type", &Event::type)
        .def_readonly("from_id", &Event::from_id)
        .def_readonly("peer_id", &Event::peer_id)
        .def_readonly("id", &Event::id)
        .def_readonly("timestamp", &Event::timestamp)
        .def_readonly("is_chat", &Event::is_chat)
        .def_readonly("words", &Event::words)
        .def_readonly("user", &Event::user)
        .def_readwrite("docs", &Event::docs)
        .def_readwrite("fwds", &Event::fwds)
        .def_readwrite("keyboard", &Event::keyboard) // JSON строка Keyboard
        .def_readonly("event_id", &Event::event_id)
        .def("send", &Event::send)      // результат как dict (json-кастер)
        .def("pysend", &Event::pysend)  // результат как JSON-строка
        .def("getOut", &Event::getOut);

    py::class_<Keyboard>(m, "Keyboard")
        .def(py::init<bool, bool>(), py::arg("one_time") = false, py::arg("inline") = false)
        .def("button", [](Keyboard& k, const std::string& label, const std::string& payload, const std::string& color) -> Keyboard&
             { return k.button(label, payload, color); },
             py::arg("label"), py::arg("payload"), py::arg("color") = "secondary", py::return_value_policy::reference)
        .def("textButton", [](Keyboard& k, const std::string& label, const std::string& color) -> Keyboard&
             { return k.textButton(label, color); },
             py::arg("label"), py::arg("color") = "secondary", py::return_value_policy::reference)
        .def("link", [](Keyboard& k, const std::string& label, const std::string& url, const std::string& color) -> Keyboard&
             { return k.link(label, url, color); },
             py::arg("label"), py::arg("url"), py::arg("color") = "secondary", py::return_value_policy::reference)
        .def("row", [](Keyboard& k) -> Keyboard& { return k.row(); }, py::return_value_policy::reference)
        .def("dump", &Keyboard::dump)
        .def("clear", &Keyboard::clear);

    py::class_<Net>(m, "Net")
        // send() возвращает str (текст) или bytes (бинарный ответ — картинка и т.п.)
        .def("send", [](Net& self, const std::string& url, const std::string& params) -> py::object
             { return pyTextOrBytes(self.send(url, params)); },
             py::arg("url"), py::arg("params") = std::string())
        .def("send", [](Net& self, const std::string& url, const table_t& params, bool post) -> py::object
             { return pyTextOrBytes(self.send(url, params, post)); },
             py::arg("url"), py::arg("params"), py::arg("post") = true)
        .def("upload",
             [](Net& self, std::string url, std::string filename, py::object data)
             {
                 std::string d = py::isinstance<py::bytes>(data) ? py::cast<std::string>(data) : std::string(py::str(data));
                 return self.upload(url, filename, d);
             },
             py::arg("url"), py::arg("filename"), py::arg("data"));

    py::class_<Vk>(m, "Vk")
        // args dict -> table_t автоматически (кастер значений через str)
        .def("send", static_cast<json (Vk::*)(const std::string&, table_t, bool)>(&Vk::send),
             py::arg("method"), py::arg("args") = table_t{}, py::arg("user") = false);

    // --- расширение API (фаза 2): файловые/строковые/временные утилиты ---
    // имена переменных не должны затенять C++ namespace-ы fs/str/timer
    py::module_ modFs = m.def_submodule("fs", "файловые утилиты");
    modFs.def("exists", [](const std::string& p) { return fs::exists(p); });
    modFs.def("readData", [](const std::string& p) { return pyTextOrBytes(fs::readData(p)); }); // binary — bytes
    modFs.def("writeData", [](const std::string& p, const std::string& d) { fs::writeData(p, d); });

    py::module_ modStr = m.def_submodule("str", "строковые утилиты (UTF-8)");
    modStr.def("words", [](const std::string& s, char d) { return str::words(s, d); }, py::arg("s"), py::arg("delim") = ' ');
    modStr.def("low", [](const std::string& s) { return str::low(s); });
    modStr.def("summ", [](py::list words, unsigned int s) { return str::summ(words.cast<args_t>(), s); });
    modStr.def("replase", [](const std::string& s, const std::string& f, const std::string& r) { return str::replase(s, f, r); });

    py::module_ modTimer = m.def_submodule("timer", "время/пауза");
    modTimer.def("sleep", [](int ms) { timer::sleep(ms); });
    modTimer.def("getWorkTime", []() { return timer::getWorkTime(); });

    // --- изображения (фаза 12): источник картинки, загрузка, фильтры OpenCV ---
    py::class_<PyImg>(m, "Image")
        .def_property_readonly("width", [](const PyImg& s) { return s.mat.cols; })
        .def_property_readonly("height", [](const PyImg& s) { return s.mat.rows; })
        .def("copy", [](const PyImg& s) { PyImg o{s.mat.clone()}; return o; })
        .def("pix", [](const PyImg& s) { return PyImg{img::toPix(s.mat)}; })       // 8bit art
        .def("crt", [](const PyImg& s) { return PyImg{img::toCrt(s.mat)}; })       // cartoon
        .def("neon", [](const PyImg& s) { return PyImg{img::toNeon(s.mat)}; })     // неон
        .def("invert", [](const PyImg& s) { return PyImg{img::toInvert(s.mat)}; }) // инверсия
        .def("gray", [](const PyImg& s) { // ч/б (в 3 канала — так же, как в фильтрах crt/neon)
            cv::Mat gray, out;
            cv::cvtColor(s.mat, gray, cv::COLOR_RGB2GRAY);
            cv::cvtColor(gray, out, cv::COLOR_GRAY2RGB);
            return PyImg{out};
        })
        .def("warp", [](const PyImg& s, int seed, int strength) { return PyImg{img::warp(s.mat, seed, strength)}; },
             py::arg("seed") = -1, py::arg("strength") = 40) // случайная деформация геометрии
        .def("scale", [](const PyImg& s, int w, int h) { return PyImg{img::scale(s.mat, w, h)}; },
             py::arg("width"), py::arg("height"))
        .def("toPng", [](const PyImg& s) { return py::bytes(s.encode(".png")); })
        .def("toJpeg", [](const PyImg& s, int q) { return py::bytes(s.encode(".jpg", q)); }, py::arg("quality") = 90);

    py::module_ modImg = m.def_submodule("img", "изображения: вложения, загрузка, фильтры");
    modImg.def("sources", [](Event& e) { return img::sources(e); }); // вложения-картинки или фото профиля
    // скачать содержимое вложения по doc.url (Doc.get() — это строка attachment, не данные!)
    modImg.def("download", [](const Doc& d, Net& net) { return pyTextOrBytes(net.send(d.url)); },
               py::arg("doc"), py::arg("net"));
    modImg.def("fromBytes", [](const std::string& data) { return PyImg::decode(data); }, py::arg("data"));
    modImg.def("fromDoc", [](const Doc& d, Net& net) { return PyImg::decode(net.send(d.url)); },
               py::arg("doc"), py::arg("net"));
    modImg.def("fromUrl", [](const std::string& url, Net& net) { return PyImg::decode(net.send(url)); },
               py::arg("url"), py::arg("net"));
}

// ---------------------------------------------------------------------------
// PyMainThread
// ---------------------------------------------------------------------------
struct PyMainThread::Impl
{
    py::scoped_interpreter guard;  // Py_Initialize; embedded-модули доступны всем интерпретаторам
    py::gil_scoped_release release; // главный поток навсегда отпускает GIL
};

PyMainThread::PyMainThread() : impl(std::make_unique<Impl>()) {}
PyMainThread::~PyMainThread() = default;

// ---------------------------------------------------------------------------
// pyF: под-интерпретатор + изолированное пространство имён на один скрипт
// ---------------------------------------------------------------------------
// Обёртка над print: каждый print отправляет ОТДЕЛЬНОЕ сообщение в чат.
// Флуд-контроль: не чаще одного сообщения в py_print_interval_ms мс (default
// 1000) и не более py_print_max_msgs сообщений на скрипт (default 30); после
// лимита — одно уведомление об обрезке, дальше print'ы игнорируются.
// print(..., file=...) не перехватывается (обычный вывод в консоль бота).

struct ScriptPrinter
{
    Event& eventOut;
    int intervalMs = 1000;
    int maxMsgs = 30;
    int sent = 0;
    bool stopped = false;
    std::chrono::steady_clock::time_point last{};

    ScriptPrinter(Event& out)
        : eventOut(out)
    {
        conf.lock.lock();
        json& c = conf.get();
        if (c["py_print_interval_ms"].is_number())
            intervalMs = c["py_print_interval_ms"].get<int>();
        if (c["py_print_max_msgs"].is_number())
            maxMsgs = c["py_print_max_msgs"].get<int>();
        conf.lock.unlock();
        if (intervalMs < 0)
            intervalMs = 0;
        if (maxMsgs < 0)
            maxMsgs = 0;
    }

    void emit(const std::string& line)
    {
        if (stopped)
            return;
        if (sent >= maxMsgs)
        {
            stopped = true;
            send("… вывод обрезан: print не более " + std::to_string(maxMsgs) + " сообщений (флуд-защита)");
            return;
        }
        if (sent > 0 && intervalMs > 0)
        {
            auto now = std::chrono::steady_clock::now();
            long long ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - last).count();
            if (ms < intervalMs)
                timer::sleep(static_cast<int>(intervalMs - ms));
        }
        last = std::chrono::steady_clock::now();
        std::string text = line.empty() ? " " : line; // VK не принимает пустые сообщения
        if (text.size() > 4000) // лимит сообщения VK — 4096, берём с запасом
            text = text.substr(0, 4000) + "…";
        send(text);
        sent++;
    }

    int printsSent() const { return sent; }

private:
    void send(const std::string& text)
    {
        table_t param;
        param["peer_id"] = std::to_string(eventOut.peer_id);
        param["random_id"] = "0";
        param["message"] = text;
        param["disable_mentions"] = "1";
        eventOut.vk.send("messages.send", param);
    }
};

static const char* PY_PRINT_PRELUDE = R"PY(
import builtins as _vk_builtins, io as _vk_io, sys as _vk_sys
_vk_print = _vk_builtins.print
def print(*a, **k):
    if 'file' in k: # явный file= (в т.ч. sys.stderr) — без отправки в чат
        _vk_print(*a, **k)
        return
    _buf = _vk_io.StringIO()
    _vk_print(*a, file=_buf, **k)
    _text = _buf.getvalue()
    _end = k.get('end', '\n')
    if _end and _text.endswith(_end):
        _text = _text[:-len(_end)]
    for _line in _text.split('\n'):
        _vk_emit(_line)
class _VkStdout:
    def write(self, s):
        s = str(s)
        for _line in s.split('\n'):
            if _line:
                _vk_emit(_line)
        return len(s)
    def flush(self):
        pass
_vk_sys.stdout = _VkStdout()
del _vk_builtins, _vk_sys # _vk_io и _vk_print нужны в момент вызова print
)PY";

struct pyF::Impl
{
    Event& eventIn;
    Event& eventOut;
    py::subinterpreter sub;
    std::unique_ptr<py::subinterpreter_scoped_activate> act;
    py::object ns;
    std::unique_ptr<ScriptPrinter> printer;

    Impl(Event& in, Event& out)
        : eventIn(in)
        , eventOut(out)
        , sub(py::subinterpreter::create())
    {
        act = std::make_unique<py::subinterpreter_scoped_activate>(sub);
        py::module_::import("vkcppbot"); // регистрирует типы в этом под-интерпретаторе
        printer = std::make_unique<ScriptPrinter>(out);
        ns = py::dict();
        ns["eventIn"] = py::cast(&eventIn, py::return_value_policy::reference);
        ns["eventOut"] = py::cast(&eventOut, py::return_value_policy::reference);
        ns["net"] = py::cast(&eventIn.net, py::return_value_policy::reference);
        ns["vk"] = py::cast(&eventIn.vk, py::return_value_policy::reference);
        ScriptPrinter* pr = printer.get();
        ns["_vk_emit"] = py::cpp_function([pr](const std::string& line) { pr->emit(line); });
        py::exec(PY_PRINT_PRELUDE, ns);
    }
};

pyF::pyF(Event& in, Event& out) : impl(std::make_unique<Impl>(in, out)) {}
pyF::~pyF() = default;

std::string pyF::exec(std::string& code)
{
    if (!impl->act) // активация могла быть снята — включаем повторно
        impl->act = std::make_unique<py::subinterpreter_scoped_activate>(impl->sub);
    try
    {
        py::exec(py::str(code), impl->ns);
    }
    catch (const py::error_already_set& e)
    {
        return e.what();
    }
    catch (const std::exception& e)
    {
        return std::string("cpp error: ") + e.what();
    }
    return "";
}

int pyF::printsSent() const
{
    return impl->printer->printsSent();
}

std::string pyF::execAddon(const std::string& source)
{
    if (!impl->act)
        impl->act = std::make_unique<py::subinterpreter_scoped_activate>(impl->sub);
    try
    {
        py::exec(py::str(source), impl->ns); // тело модуля аддона (импорты, метаданные)
        if (!impl->ns.contains("run"))
            return "аддон не определяет run(eventIn, eventOut, net, vk)";
        py::object res = impl->ns["run"](impl->ns["eventIn"], impl->ns["eventOut"],
                                         impl->ns["net"], impl->ns["vk"]);
        if (!res.is_none()) // строка из run() дописывается к ответу
        {
            std::string s = py::str(res);
            if (!s.empty())
                impl->eventOut.msg += s;
        }
    }
    catch (const py::error_already_set& e)
    {
        return e.what();
    }
    catch (const std::exception& e)
    {
        return std::string("cpp error: ") + e.what();
    }
    return "";
}

// ---------------------------------------------------------------------------
// Метаданные python-аддона: временный под-интерпретатор, exec исходника,
// чтение cmd/info/acess/cat + проверка наличия run(). Ошибки импорта/валидации —
// в error (аддон пропускается, бот не падает).
// ---------------------------------------------------------------------------
PyAddonMeta pyAddonMeta(const std::string& source)
{
    PyAddonMeta m;
    // ВАЖНО: sub/act/ns — вне try. Если объявить их в try, то при исключении
    // стек развернётся до catch вместе с под-интерпретатором, а error_already_set
    // будет держать ссылки на уже освобождённые объекты Python (double free —
    // найдено тестом T17f).
    py::subinterpreter sub = py::subinterpreter::create();
    py::subinterpreter_scoped_activate act(sub);
    py::dict ns;
    ns["_vk_emit"] = py::cpp_function([](const std::string&) {}); // print при импорте — в пустоту
    try
    {
        py::exec(PY_PRINT_PRELUDE, ns);
        py::exec(py::str(source), ns);
        if (!ns.contains("cmd") || !py::isinstance<py::str>(ns["cmd"]))
        {
            m.error = "нет строки cmd = \"/команда\"";
            return m;
        }
        m.cmd = ns["cmd"].cast<std::string>();
        if (m.cmd.size() < 2 || m.cmd[0] != '/')
        {
            m.error = "cmd должен начинаться с «/» и быть не короче 2 символов";
            return m;
        }
        for (char c : m.cmd)
            if (static_cast<unsigned char>(c) <= ' ')
            {
                m.error = "cmd не должен содержать пробелов";
                return m;
            }
        if (!ns.contains("info") || !py::isinstance<py::str>(ns["info"]))
        {
            m.error = "нет строки info (описание для /help)";
            return m;
        }
        m.info = ns["info"].cast<std::string>();
        if (!ns.contains("acess") || !py::isinstance<py::int_>(ns["acess"]))
        {
            m.error = "нет целого acess (уровень прав 1..5)";
            return m;
        }
        m.acess = ns["acess"].cast<int>();
        if (m.acess < 1 || m.acess > 5)
        {
            m.error = "acess вне диапазона 1..5";
            return m;
        }
        if (!ns.contains("cat") || !py::isinstance<py::str>(ns["cat"]))
        {
            m.error = "нет строки cat (раздел справки)";
            return m;
        }
        m.cat = ns["cat"].cast<std::string>();
        if (!ns.contains("run") || !PyCallable_Check(ns["run"].ptr()))
        {
            m.error = "нет функции run(eventIn, eventOut, net, vk)";
            return m;
        }
    }
    catch (const py::error_already_set& e)
    {
        m.error = std::string("импорт: ") + e.what();
    }
    catch (const std::exception& e)
    {
        m.error = std::string("cpp: ") + e.what();
    }
    return m;
}

// ---------------------------------------------------------------------------
// Вики по python-скриптам: текст разделён на print-разделы — каждый уходит
// отдельным сообщением (со штатным флуд-контролем ScriptPrinter)
// ---------------------------------------------------------------------------
const char* pyWikiScript()
{
    static const char* wiki = R"PY(
print("Wiki /py (1/8). Python-скрипты прямо в чате: /py <код>. Каждый запуск — изолированный под-интерпретатор, свои переменные, таймаутов нет; код одной строкой, операторы через «;»")
print("(2/8) Переменные: eventIn — входящее событие: msg, words, from_id, peer_id, is_chat, user (name/id/acess/msgs), docs, fwds; eventOut — ответ: msg, keyboard; net — HTTP: net.send(url, params); vk — VK API: vk.send('users.get', {'user_ids': 1})")
print("(3/8) Модуль vkcppbot: import vkcppbot; vkcppbot.fs.exists/readData/writeData; vkcppbot.str.words/low/summ/replase; vkcppbot.timer.sleep(мс)/getWorkTime()")
print("(4/8) Классы: Keyboard(one_time, inline): button(подпись, payload)/textButton/link/row/dump; Doc(): uploadDoc(файл, данные, net, vk, peer_id)/uploadDoc с audio_message=True/uploadPhoto; Event: getOut/send/pysend")
print("(5/8) print() отправляет отдельное сообщение в чат (sys.stdout — тоже). Флуд-защита: не чаще 1 сообщения в секунду (py_print_interval_ms) и максимум 30 на скрипт (py_print_max_msgs), обе настройки в config.json")
print("(6/8) Изображения: vkcppbot.img.sources(eventIn) — картинка-вложение или фото профиля; img.download(doc, net) — данные вложения (bytes); img.fromDoc(doc, net)/img.fromUrl(url, net)/img.fromBytes(bytes) → Image: pix()/crt()/neon()/gray()/invert()/warp(seed=-1, сила=40) — случайная деформация геометрии (seed >= 0 — повторяемо)/scale(w,h)/toPng()/toJpeg(), width/height; загрузка — Doc().uploadPhoto(имя, bytes, net, vk, peer_id); net.send(url) для бинарного ответа вернёт bytes")
print("(7/8) Примеры: /py import vkcppbot; print(vkcppbot.str.low(eventIn.msg)) ; /py print(eventIn.user.name + ', ' + str(len(eventIn.words)) + ' слов')")
print("(8/8) Ещё: /py eventOut.msg = 'привет' — обычный ответ без print ; /py eventOut.keyboard = vkcppbot.Keyboard().button('да', '/yes').dump() — клавиатура; свои ПОСТОЯННЫЕ команды — файлы в каталоге py_addons/ (cmd/info/acess/cat + def run(eventIn, eventOut, net, vk)), список — /addons, перечитать — /addonreload; /pywiki — эта справка заново")
)PY";
    return wiki;
}
