#pragma once
#include "doc.h"
#include "vk.h"
#include <opencv2/imgcodecs/imgcodecs.hpp>
#include <opencv2/imgproc/imgproc.hpp>

#ifdef WIN32
#pragma comment(lib, "libgd.lib")
#endif

class img
{
public:
    img();
    img(cv::Mat New);
    img(Doc doc, Net& net, bool full = false);
    Doc getDoc(uint32_t peer_id, Net& net, Vk& vk);
    Doc getPhoto(uint32_t peer_id, Net& net, Vk& vk);
    cv::Mat im;
};

//color transformations

typedef struct
{
    double r; // a fraction between 0 and 1
    double g; // a fraction between 0 and 1
    double b; // a fraction between 0 and 1
} rgb_t;

typedef struct
{
    double h; // angle in degrees
    double s; // a fraction between 0 and 1
    double v; // a fraction between 0 and 1
} hsv_t;

hsv_t rgb2hsv(rgb_t in);
rgb_t hsv2rgb(hsv_t in);
