#pragma once
#include "doc.h"
#include "vk.h"
#include <gd.h>
#include <opencv2/imgcodecs/imgcodecs.hpp>
#include <opencv2/imgproc/imgproc.hpp>

#ifdef WIN32
#pragma comment(lib, "libgd.lib")
#endif

class img
{
public:
    img(int sx, int sy);
    img(gdImagePtr New);
    img(Doc doc, Net& net);
    ~img();
    img copy();
    img copyCrop(uint32_t x, uint32_t y, uint32_t sx, uint32_t sy); //влом пока
    std::string getPng();
    Doc getDoc(uint32_t peer_id, Net& net, Vk& vk);
    Doc getPhoto(uint32_t peer_id, Net& net, Vk& vk);
    gdImagePtr im;

    static Doc CVtoPhoto(cv::Mat matIm, uint32_t peer_id, Net& net, Vk& vk);
    static Doc CVtoDoc(cv::Mat matIm, uint32_t peer_id, Net& net, Vk& vk);
    cv::Mat getCVim();
};
