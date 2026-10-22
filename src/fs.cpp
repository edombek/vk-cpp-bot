#include "fs.h"
#include "common.h"
#include <stdio.h>
#include <sstream>

using namespace std;

bool fs::exists(const string& path)
{
    ifstream fin;
    fin.open(path);
    if (fin.is_open())
    {
        fin.close();
        return true;
    }
    return false;
}

/* --------- FILE CLASS ---------*/

fs::file* fs::open(const string& path, int flags)
{
    file* _file = new file();
    _file->getStream()->open(path, (ios_base::openmode)flags);
    return _file;
}

void fs::file::close()
{
    this->_stream.close();
    delete this;
}

string fs::file::read()
{
    // точное чтение всего потока (старый построчный вариант терял
    // завершающий '\n' и портил бинарные/CR-LF данные)
    std::ostringstream ss;
    ss << this->_stream.rdbuf();
    return ss.str();
}

void fs::file::readBinary(char* data, size_t size)
{
    this->_stream.read(data, size);
}

fstream* fs::file::getStream()
{
    return &this->_stream;
}

void fs::file::write(const string& data)
{
    this->_stream << data;
}

void fs::file::writeBinary(const char* data, size_t size)
{
    this->_stream.write(data, size);
}

/* --------- /FILE CLASS/ ---------*/

void fs::writeData(const string& path, const string& data)
{
    if (fs::file* file = fs::open(path, FS_WRITE))
    {
        file->write(data);
        file->close();
    }
}

void fs::safeWriteData(const string& path, const string& data)
{
    fs::writeData(path + ".safe", data);
    remove(path.c_str());
    fs::rename(path + ".safe", path);
}

string fs::readData(const string& path)
{
    string data;
    fs::file* in = fs::open(path);
    data = in->read();
    in->close();
    return data;
}

void fs::rename(const string& oldname, const string& newname)
{
    ::rename(oldname.c_str(), newname.c_str());
}

/* CLASS EXCEPTION */
fs::exception::exception(const string& what)
{
    this->what_str = what;
}

string fs::exception::what() const
{
    return this->what_str;
}
