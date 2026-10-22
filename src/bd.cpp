#include "bd.h"
#include "fs.h"

BD::BD(const std::string& path)
{
    this->file = path;
    if (fs::exists(path))
        this->dat = json::parse(fs::readData(path));
}

BD::~BD()
{
    this->saveIfDirty();
}

json& BD::get()
{
    return this->dat;
}

void BD::touch()
{
    this->lock.lock();
    this->dirty = true;
    this->lock.unlock();
}

void BD::save()
{
    this->lock.lock();
    fs::writeData(this->file, this->dat.dump(4));
    this->dirty = false;
    this->lock.unlock();
}

bool BD::saveIfDirty()
{
    this->lock.lock();
    bool d = this->dirty;
    this->lock.unlock();
    if (d)
        this->save();
    return d;
}
