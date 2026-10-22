#pragma once
// pybind11 type casters между типами бота и Python:
//   table_t (map<string,string>) <-> dict — значения приводятся через str(),
//     поэтому int/bool из скрипта больше не роняют бота (старый extract<string>)
//   nlohmann::json <-> dict/list/скаляры (аддон pybind11/json.h в pybind11 3.x удалён)
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "common.h"

namespace pybind11
{
namespace detail
{

namespace py = pybind11;

template <>
struct type_caster<table_t>
{
public:
    PYBIND11_TYPE_CASTER(table_t, const_name("dict"));

    bool load(handle src, bool)
    {
        if (!PyDict_Check(src.ptr()))
            return false;
        value = {};
        PyObject* key = nullptr;
        PyObject* val = nullptr;
        Py_ssize_t pos = 0;
        while (PyDict_Next(src.ptr(), &pos, &key, &val))
        {
            py::object k = reinterpret_borrow<py::object>(key);
            py::object v = reinterpret_borrow<py::object>(val);
            value[k.cast<std::string>()] = py::str(v).cast<std::string>();
        }
        return true;
    }

    static handle cast(const table_t& src, return_value_policy, handle)
    {
        dict d;
        for (const auto& kv : src)
            d[py::str(kv.first)] = py::str(kv.second);
        return d.release();
    }
};

template <>
struct type_caster<json>
{
public:
    PYBIND11_TYPE_CASTER(json, const_name("object"));

    static py::object toPy(const json& j)
    {
        switch (j.type())
        {
        case json::value_t::null:
            return py::none();
        case json::value_t::boolean:
            return py::bool_(j.get<bool>());
        case json::value_t::number_integer:
        case json::value_t::number_unsigned:
            return py::int_(j.get<long long>());
        case json::value_t::number_float:
            return py::float_(j.get<double>());
        case json::value_t::string:
            return py::str(j.get<std::string>());
        case json::value_t::array:
        {
            py::list l;
            for (const auto& e : j)
                l.append(toPy(e));
            return l;
        }
        case json::value_t::object:
        {
            py::dict d;
            for (auto it = j.begin(); it != j.end(); ++it)
                d[py::str(it.key())] = toPy(it.value());
            return d;
        }
        default: // binary и пр.
            throw py::type_error("vkcppbot: json type not supported for python");
        }
    }

    static json fromPy(py::handle src)
    {
        PyObject* o = src.ptr();
        if (o == Py_None)
            return json();
        if (PyBool_Check(o))
            return json(o == Py_True);
        if (PyLong_Check(o))
        {
            long long v = PyLong_AsLongLong(o);
            if (v == -1 && PyErr_Occurred()) // гигантские int: не падаем, а во float
            {
                PyErr_Clear();
                return json(PyLong_AsDouble(o));
            }
            return json(v);
        }
        if (PyFloat_Check(o))
            return json(PyFloat_AsDouble(o));
        if (PyUnicode_Check(o))
        {
            Py_ssize_t sz = 0;
            const char* s = PyUnicode_AsUTF8AndSize(o, &sz);
            if (!s)
                throw py::error_already_set();
            return json(std::string(s, (size_t)sz));
        }
        // bytes -> str через py::str ниже (binary_t в json 3.4 нет;
        // для бинарных аргументов функций используйте std::string — pybind11 примет bytes)
        if (PyList_Check(o) || PyTuple_Check(o))
        {
            json arr = json::array();
            bool islist = PyList_Check(o);
            Py_ssize_t n = islist ? PyList_GET_SIZE(o) : PyTuple_GET_SIZE(o);
            for (Py_ssize_t i = 0; i < n; i++)
            {
                PyObject* item = islist ? PyList_GET_ITEM(o, i) : PyTuple_GET_ITEM(o, i);
                arr.push_back(fromPy(reinterpret_borrow<py::object>(item)));
            }
            return arr;
        }
        if (PyDict_Check(o))
        {
            json obj = json::object();
            PyObject* key = nullptr;
            PyObject* val = nullptr;
            Py_ssize_t pos = 0;
            while (PyDict_Next(o, &pos, &key, &val))
            {
                std::string k = py::str(reinterpret_borrow<py::object>(key)).cast<std::string>();
                obj[k] = fromPy(reinterpret_borrow<py::object>(val));
            }
            return obj;
        }
        return json(py::str(src).cast<std::string>()); // любой прочий объект — через repr-строку
    }

    bool load(handle src, bool)
    {
        value = fromPy(src);
        return true;
    }

    static handle cast(const json& src, return_value_policy, handle)
    {
        return toPy(src).release();
    }
};

} // namespace detail
} // namespace pybind11
