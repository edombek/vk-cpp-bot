#pragma once
#include "doc.h"
#include "vk.h"
#ifdef WIN32
#include <opencv2/highgui/highgui.hpp>
#else
#include <opencv2/imgcodecs/imgcodecs.hpp>
#endif
#include <opencv2/imgproc/imgproc.hpp>

class img
{
public:
    bool operator==(const img& im) const
    {
        return this->doc.get()==im.doc.get();
    };
    img& operator=(const img& im)
    {
        if (this != &im) // ссылка net не переприсваивается — копируем doc
            this->doc = im.doc;
        return *this;
    };

    img(const Doc& doc, Net& n);
    cv::Mat getMat(bool full = false);
    static Doc getDoc(cv::Mat im, int64_t peer_id, Net& net, Vk& vk);
    static Doc getPhoto(cv::Mat im, int64_t peer_id, Net& net, Vk& vk);
    static bool isBig(cv::Mat im);
    // «неоновая» цветокоррекция: H и S сохраняются, V принудительно 1, чёрные
    // пиксели не трогаются. Векторный эквивалент старого попиксельного цикла
    // (точность — ±1 LSB из-за другого порядка округления).
    static cv::Mat neonColors(const cv::Mat& src);
    Doc doc;
    Net &net;
};
