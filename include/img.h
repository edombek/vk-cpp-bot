#ifndef IMG_H
#define IMG_H
#include "doc.h"
#include "vk.h"
#include <gd.h>

class img {
public:
    img(int sx, int xy);
    img(gdImagePtr New);
    img(Doc* doc, Net* net);
    ~img();
    img copy();
    std::string getPng();
    Doc* getDoc(uint32_t peer_id, Net* net, Vk* vk);
    Doc* getPhoto(uint32_t peer_id, Net* net, Vk* vk);
    gdImagePtr im;
};

#endif // IMG_H
