#include "img.h"

img::img(int sx, int sy)
{
    this->im = gdImageCreateTrueColor(sx, sy);
}

img::img(gdImagePtr New)
{
    this->im = New;
}

img::img(Doc* doc, Net* net)
{
    this->im = NULL;
    net->send(doc->url);
    if (doc->ext == "jpg" || doc->ext == "jpeg" || doc->ext == "JPG" || doc->ext == "")
        this->im = gdImageCreateFromJpegPtr(net->buffer.size(), (void*)net->buffer.c_str());
    else if (doc->ext == "png")
        this->im = gdImageCreateFromPngPtr(net->buffer.size(), (void*)net->buffer.c_str());
    else if (doc->ext == "bmp" || doc->ext == "BMP")
        this->im = gdImageCreateFromBmpPtr(net->buffer.size(), (void*)net->buffer.c_str());
}

std::string img::getPng()
{
    int s;
    void* png = gdImagePngPtrEx(this->im, &s, 0);
    return std::string((const char*)png, s);
}

Doc* img::getDoc(std::string type, std::string name, uint32_t peer_id, Net* net, Vk* vk)
{
    if (this->im == NULL)
        return NULL;
    std::string dat = this->getPng();
    return new Doc(type, std::to_string(peer_id), name + ".png", dat, net, vk);
}

img::~img()
{
    if (this->im)
        gdImageDestroy(this->im);
}
