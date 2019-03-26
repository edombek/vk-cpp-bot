#ifndef IMG_H
#define IMG_H
#include "doc.h"
#include "vk.h"
#include <gd.h>

#ifdef WIN32
#pragma comment(lib, "libgd.lib")
#endif

class img {
public:
    img(int sx, int xy);
    img(gdImagePtr New);
    img(Doc doc, Net& net);
    ~img();
    img copy();
    img copyCrop(uint32_t x, uint32_t y, uint32_t sx, uint32_t sy); //влом пока
    std::string getPng();
    Doc getDoc(uint32_t peer_id, Net& net, Vk& vk);
    Doc getPhoto(uint32_t peer_id, Net& net, Vk& vk);
    gdImagePtr im;
};

#endif // IMG_H
