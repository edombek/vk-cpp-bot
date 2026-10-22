#pragma once
#include <fstream>

#define FS_READ std::ios::in
#define FS_WRITE std::ios::out
#define FS_BINARY std::ios::binary

namespace fs
{
class file
{
private:
    std::fstream _stream;

public:
    std::fstream* getStream();
    void write(const std::string& data);
    void writeBinary(const char* data, size_t size);
    std::string read();
    void readBinary(char* data, size_t size);
    void close();
};

class exception
{
    std::string what_str;

public:
    exception(const std::string& what);
    std::string what() const;
};

bool exists(const std::string& path);

file* open(const std::string& path, int flags = FS_READ | FS_WRITE);

void writeData(const std::string& path, const std::string& data);
void safeWriteData(const std::string& path, const std::string& data);
std::string readData(const std::string& path);
void rename(const std::string& oldname, const std::string& newname);
}
