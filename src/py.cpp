#include "py.h"

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
#include "keyboard.h"
#include "net.h"
#include "str.h"
#include "timer.h"
#include "users.h"
#include "vk.h"

namespace py = pybind11;

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
        .def("get", &Doc::get)
        .def("uploadDoc",
             [](Doc& self, std::string filename, py::object data, Net& net, Vk& vk, uint32_t peer_id, bool audio)
             {
                 std::string d = py::isinstance<py::bytes>(data) ? py::cast<std::string>(data) : std::string(py::str(data));
                 return self.uploadDoc(filename, d, net, vk, peer_id, audio);
             },
             py::arg("filename"), py::arg("data"), py::arg("net"), py::arg("vk"), py::arg("peer_id"), py::arg("audio_message") = false)
        .def("uploadPhoto",
             [](Doc& self, std::string filename, py::object data, Net& net, Vk& vk, uint32_t peer_id)
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
        .def("send", py::overload_cast<std::string, std::string>(&Net::send),
             py::arg("url"), py::arg("params") = std::string())
        .def("send", py::overload_cast<std::string, table_t, bool>(&Net::send),
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
        .def("send", static_cast<json (Vk::*)(std::string, table_t, bool)>(&Vk::send),
             py::arg("method"), py::arg("args") = table_t{}, py::arg("user") = false);

    // --- расширение API (фаза 2): файловые/строковые/временные утилиты ---
    // имена переменных не должны затенять C++ namespace-ы fs/str/timer
    py::module_ modFs = m.def_submodule("fs", "файловые утилиты");
    modFs.def("exists", [](const std::string& p) { return fs::exists(p); });
    modFs.def("readData", [](const std::string& p) { return fs::readData(p); });
    modFs.def("writeData", [](const std::string& p, const std::string& d) { fs::writeData(p, d); });

    py::module_ modStr = m.def_submodule("str", "строковые утилиты (UTF-8)");
    modStr.def("words", [](const std::string& s, char d) { return str::words(s, d); }, py::arg("s"), py::arg("delim") = ' ');
    modStr.def("low", [](const std::string& s) { return str::low(s); });
    modStr.def("summ", [](py::list words, unsigned int s) { return str::summ(words.cast<args_t>(), s); });
    modStr.def("replase", [](const std::string& s, const std::string& f, const std::string& r) { return str::replase(s, f, r); });

    py::module_ modTimer = m.def_submodule("timer", "время/пауза");
    modTimer.def("sleep", [](int ms) { timer::sleep(ms); });
    modTimer.def("getWorkTime", []() { return timer::getWorkTime(); });
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
struct pyF::Impl
{
    Event& eventIn;
    Event& eventOut;
    py::subinterpreter sub;
    std::unique_ptr<py::subinterpreter_scoped_activate> act;
    py::object ns;

    Impl(Event& in, Event& out)
        : eventIn(in)
        , eventOut(out)
        , sub(py::subinterpreter::create())
    {
        act = std::make_unique<py::subinterpreter_scoped_activate>(sub);
        py::module_::import("vkcppbot"); // регистрирует типы в этом под-интерпретаторе
        ns = py::dict();
        ns["eventIn"] = py::cast(&eventIn, py::return_value_policy::reference);
        ns["eventOut"] = py::cast(&eventOut, py::return_value_policy::reference);
        ns["net"] = py::cast(&eventOut.net, py::return_value_policy::reference);
        ns["vk"] = py::cast(&eventOut.vk, py::return_value_policy::reference);
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
