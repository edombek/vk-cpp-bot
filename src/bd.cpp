#include "bd.h"
#include "fs.h"

BD::BD(std::string path)
{
    this->file = path;
    if (fs::exists(path))
        this->dat = json::parse(fs::readData(path));
}

BD::~BD()
{
    this->save();
}

json& BD::get()
{
    return this->dat;
}

void BD::save()
{
    this->lock.lock();
    fs::writeData(this->file, this->dat.dump(4));
    this->lock.unlock();
}
