#include "img.h"
#include "vk.h"

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

Doc* img::getDoc(uint32_t peer_id, Net* net, Vk* vk)
{
    if (this->im == NULL)
        return NULL;
    std::string dat = this->getPng();
    Doc* doc = new Doc();
    if (doc->uploadDoc("img.png", dat, net, vk, peer_id))
        return doc;
    delete doc;
    return NULL;
}

Doc* img::getPhoto(uint32_t peer_id, Net* net, Vk* vk)
{
    if (this->im == NULL)
        return NULL;
    std::string dat = this->getPng();
    Doc* doc = new Doc();
    if (doc->uploadPhoto("img.png", dat, net, vk, peer_id))
        return doc;
    delete doc;
    return NULL;
}

img::~img()
{
    if (this->im)
        gdImageDestroy(this->im);
}

img img::copy()
{
    return img(gdImageClone(this->im));
}
