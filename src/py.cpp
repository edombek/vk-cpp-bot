#include <py.h>

pyF::pyF(Event e): event(e) {}

py::dict pyF::toPythonDict(table_t map)
{
    typename table_t::iterator iter;
    py::dict dictionary;
    for (iter = map.begin(); iter != map.end(); ++iter)
    {
        dictionary[iter->first] = iter->second;
    }
    return dictionary;
}

table_t pyF::toTable(py::dict dict)
{
    table_t dic;
    for (unsigned int i = 0; i < py::len(dict.keys()); i++)
        dic[py::extract<std::string>(dict.keys()[i])] = py::extract<std::string>(dict[dict.keys()[i]]);
    return dic;
}

py::list pyF::toPythonList(args_t a)
{
    py::list out;
    for (auto s : a)
        out.append(s);
    return out;
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
    py::object main_module = py::import("__main__");
    py::object main_namespace = main_module.attr("__dict__");
    boost::function<void(std::string)> _pyprint = boost::bind( &pyF::print, this, _1);
    try
    {
        void (*pyprint) (py::object obj) = [](py::object obj)
        {
            _pyprint(py::extract<std::string>(py::str(obj)));
        };
        main_module.attr("print") = pyprint;
        py::exec(py::str(code), main_namespace);
    }
    catch (py::error_already_set&)
    {
        return this->error();
    }
    return "";
}

void pyF::print(std::string str)
{
    this->event.msg = str;
    this->event.send();
}
