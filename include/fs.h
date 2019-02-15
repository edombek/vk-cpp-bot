#ifndef FS_H_INCLUDED
#define FS_H_INCLUDED
#include <fstream>

#define FS_READ std::ios::in
#define FS_WRITE std::ios::out
#define FS_BINARY std::ios::binary

namespace fs {
class file {
private:
    std::fstream _stream;

public:
    std::fstream* getStream();
    void write(std::string data);
    void writeBinary(const char* data, size_t size);
    std::string read();
    void readBinary(char* data, size_t size);
    void close();
};

class exception {
    std::string what_str;

public:
    exception(std::string what);
    std::string what();
};

bool exists(std::string path);

file* open(std::string path, int flags = FS_READ | FS_WRITE);

void writeData(std::string path, std::string data);
void safeWriteData(std::string path, std::string data);
std::string readData(std::string path);
std::string getRootPath();
void rename(std::string oldname, std::string newname);
}

#endif
