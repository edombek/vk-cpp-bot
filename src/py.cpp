#include <py.h>
#include <boost/python/suite/indexing/map_indexing_suite.hpp>
#include <boost/python/suite/indexing/vector_indexing_suite.hpp>

#pragma comment(lib, "boost_python37-vc140-mt-x32-1_68.lib")
#pragma comment(lib, "python3.lib")

using namespace boost::python;

BOOST_PYTHON_MODULE(vkcppbot)
{
    class_<std::vector<Event>>("Events")
    .def(vector_indexing_suite<std::vector<Event>>())
    .def("append", static_cast<void (std::vector<Event>::*)(const Event&)>( &std::vector<Event>::push_back ))
    ;
    class_<std::vector<Doc>>("Docs")
    .def(vector_indexing_suite<std::vector<Doc>>())
    .def("append", static_cast<void (std::vector<Doc>::*)(const Doc&)>( &std::vector<Doc>::push_back ))
    ;


    class_<Net>("Net", no_init)
    .def("send", static_cast<std::string(Net::*)(std::string, std::string)>( &Net::send ))
    .def("upload", &Net::upload)
    ;

    class_<Vk>("Vk", no_init)
    .def("send", static_cast<std::string(Vk::*)(std::string, dict, bool)>( &Vk::send ))
    ;

    class_<Event>("Event", no_init)
    .add_property("user", &Event::user)
    .def_readwrite("docs", &Event::docs)
    .def_readwrite("fwds", &Event::fwds)


    .add_property("type", &Event::type)
    .def_readwrite("msg", &Event::msg)
    .add_property("words", &Event::words)
    .add_property("from_id", &Event::from_id)
    .def_readonly("peer_id", &Event::peer_id)
    .add_property("id", &Event::id)
    .add_property("timestamp", &Event::timestamp)
    .add_property("is_chat", &Event::is_chat)

    .def("send", &Event::pysend)
    ;

    class_<users::user>("user", no_init)
    .add_property("name", &users::user::name)
    .add_property("id", &users::user::id)
    .add_property("acess", &users::user::acess)
    .add_property("msgs", &users::user::msgs)
    ;

    class_<Doc>("Doc", init<std::string>())
    .add_property("type", &Doc::type)
    .add_property("owner_id", &Doc::owner_id)
    .add_property("doc_id", &Doc::doc_id)
    .add_property("acess_key", &Doc::acess_key)
    .add_property("ext", &Doc::ext)
    .add_property("url", &Doc::url)
    .add_property("timestamp", &Doc::timestamp)

    .def("get", &Doc::get)
    .def("uploadDoc", &Doc::uploadDoc)
    .def("uploadPhoto", &Doc::uploadPhoto)
    ;
};

void pyF::init()
{
    PyInit_vkcppbot();
}

pyF::pyF(Event &in, Event &out): eventIn(in), eventOut(out)
{
    this->main_module = py::import("__main__");
    this->main_namespace = main_module.attr("__dict__");
    this->main_module.attr("eventIn") = boost::python::ptr(&eventIn);
    this->main_module.attr("eventOut") = boost::python::ptr(&eventOut);
    this->main_module.attr("net") = boost::python::ptr(&eventOut.net);
    this->main_module.attr("vk") = boost::python::ptr(&eventOut.vk);
}

std::string pyF::error()
{
    PyObject *exc, *val, *tb;
    PyErr_Fetch(&exc, &val, &tb);
    PyErr_NormalizeException(&exc, &val, &tb);
    py::handle<> hexc(exc), hval(py::allow_null(val)), htb(py::allow_null(tb));
    py::object format_exception = py::import("traceback").attr("format_exception");
    return py::extract<std::string>(py::str("").join(format_exception(hexc, hval, htb)));
}

std::string pyF::exec(std::string &code)
{
    try
    {
        py::exec(py::str(code), main_namespace);
    }
    catch (py::error_already_set&)
    {
        return this->error();
    }
    return "";
}

py::dict tabl2map(table_t table)
{
    typename table_t::iterator iter;
    py::dict dictionary;
    for (iter = table.begin(); iter != table.end(); ++iter)
    {
        dictionary[iter->first] = iter->second;
    }
    return dictionary;
}

table_t dict2map(py::dict dict)
{
    table_t dic;
    for (int i = 0; i < py::len(dict.keys()); i++)
        dic[py::extract<std::string>(dict.keys()[i])] = py::extract<std::string>(dict[dict.keys()[i]]);
    return dic;
}
