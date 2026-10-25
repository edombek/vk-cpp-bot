#include "img.h"
#include "vk.h"
#include <algorithm>

#if CV_VERSION_MAJOR >= 5
// OpenCV 5: getPerspectiveTransform переехал в модуль geometry (libopencv_geometry)
#include <opencv2/geometry/2d.hpp>
#endif

#ifdef WIN32
#pragma comment(lib, "opencv_core2413.lib")
#pragma comment(lib, "opencv_imgproc2413.lib")
#pragma comment(lib, "opencv_highgui2413.lib")
#endif

// OpenCV 5 убрал константы с префиксом CV_ (значения те же — 0)
#ifndef CV_ADAPTIVE_THRESH_MEAN_C
#define CV_ADAPTIVE_THRESH_MEAN_C 0
#endif
#ifndef CV_THRESH_BINARY
#define CV_THRESH_BINARY 0
#endif

#include "str.h"
img::img(const Doc& d, Net& n):doc(d), net(n){}

cv::Mat img::getMat(bool full)
{
    std::string buff = this->net.send(doc.url);
    std::vector<uchar> vectordata(buff.begin(),buff.end());
    cv::Mat data_mat(vectordata,true);
    cv::Mat imtemp = cv::imdecode(data_mat,1);
    if (!full && this->isBig(imtemp))
    {
        float sf = 2560.0/MAX(imtemp.size().width, imtemp.size().height);
        cv::resize(imtemp, imtemp, cv::Size(), sf, sf);
    }
    return imtemp;
}

Doc img::getDoc(cv::Mat im, int64_t peer_id, Net& net, Vk& vk)
{
    std::vector<uchar> buff;
    cv::imencode(".png", im, buff, {});
    std::string dat(buff.begin(), buff.end());
    Doc doctemp;
    if (doctemp.uploadDoc("img.png", dat, net, vk, peer_id))
        return doctemp;
    return doctemp;
}

Doc img::getPhoto(cv::Mat im, int64_t peer_id, Net& net, Vk& vk)
{
    std::vector<uchar> buff;
    cv::imencode(".jpg", im, buff, {});
    std::string dat(buff.begin(), buff.end());
    Doc doctemp;
    if (doctemp.uploadPhoto("img.jpg", dat, net, vk, peer_id))
        return doctemp;
    return doctemp;
}

bool img::isBig(cv::Mat im)
{
    if(im.empty()) return false;
    return MAX(im.size().width, im.size().height) > 2560;
}

//color transformations

cv::Mat img::neonColors(const cv::Mat& src)
{
    if (src.empty())
        return src;
    cv::Mat flt;
    src.convertTo(flt, CV_32FC3, 1.0 / 255.0);
    cv::Mat hsv;
    cv::cvtColor(flt, hsv, cv::COLOR_BGR2HSV_FULL); // H в диапазоне [0,1)
    std::vector<cv::Mat> ch;
    cv::split(hsv, ch);
    cv::Mat black; // маска V==0 — чёрные пиксели (их старый цикл пропускал)
    cv::compare(ch[2], 0.0, black, cv::CMP_EQ);
    ch[2] = cv::Mat(ch[0].size(), CV_32F, cv::Scalar(1.0)); // V принудительно 1
    cv::Mat hsvOut;
    cv::merge(ch, hsvOut);
    cv::Mat back;
    cv::cvtColor(hsvOut, back, cv::COLOR_HSV2BGR_FULL);
    cv::Mat out;
    back.convertTo(out, CV_8UC3, 255.0);
    src.copyTo(out, black); // чёрные остаются как были
    return out;
}

// источник картинки для арт-команд: вложения сообщения/комментария (фото или документ),
// а если вложений нет — фотография профиля автора (сообщества — его обложка)
std::vector<Doc> img::sources(Event &eventIn)
{
    if (!eventIn.docs.empty())
        return eventIn.docs;
    json resp;
    if (eventIn.from_id > 0)
        resp = eventIn.vk.send("users.get", { { "user_ids", std::to_string(eventIn.from_id) },
                                              { "fields", "photo_200,photo_100,photo_50" } })["response"];
    else
        resp = eventIn.vk.send("groups.getById", { { "group_id", std::to_string(-eventIn.from_id) },
                                                  { "fields", "photo_200,photo_100,photo_50" } })["response"];
    if (!resp.is_array() || resp.empty())
        return {};
    for (const char* f : { "photo_200", "photo_100", "photo_50" })
        if (resp[0][f].is_string() && !std::string(resp[0][f]).empty())
        {
            Doc d;
            d.type = "photo";
            d.url = resp[0][f];
            return { d };
        }
    return {};
}

#define imgdelta 8.0

cv::Mat img::toPix(const cv::Mat& src)
{
    if (src.empty())
        return src;
    cv::Mat out = src.clone();
    cv::resize(out, out, cv::Size(), 1/imgdelta, 1/imgdelta);
    cv::resize(out, out, cv::Size(), imgdelta, imgdelta, cv::INTER_NEAREST);
    return out;
}

#define nDownSampling 2
#define nBts 7

cv::Mat img::toCrt(const cv::Mat& src)
{
    if (src.empty())
        return src;
    cv::Mat cvim = src.clone();
    cv::Mat imgColored = cvim.clone();
    for (int i = 0; i < nDownSampling; i++)
        cv::pyrDown(imgColored, imgColored);
    for (int i = 0; i < nBts; i++)
    {
        cv::Mat buff;
        cv::bilateralFilter(imgColored, buff, 9, 9, 7);
        imgColored = std::move(buff); // без лишнего clone
    }
    for (int i = 0; i < nDownSampling; i++)
        cv::pyrUp(imgColored, imgColored);
    cv::Mat imgGray;
    cv::cvtColor(imgColored, imgGray, cv::COLOR_RGB2GRAY);
    cv::Mat imgBlur;
    cv::medianBlur(imgGray, imgBlur, 7);
    cv::Mat imgEdge;
    cv::adaptiveThreshold(imgBlur, imgEdge, 255, CV_ADAPTIVE_THRESH_MEAN_C, CV_THRESH_BINARY, 9, 2);
    cv::cvtColor(imgEdge, imgEdge, cv::COLOR_GRAY2RGB);
    cv::resize(imgEdge, imgEdge, cv::Size(imgColored.size().width, imgColored.size().height));
    cv::Rect myROI(0, 0, std::min(cvim.size().width, imgEdge.size().width), std::min(cvim.size().height, imgEdge.size().height));
    cvim = cvim(myROI);
    imgEdge = imgEdge(myROI);
    cv::bitwise_and(imgColored, imgEdge, cvim);
    return cvim; // прежнее поведение cmds_media: результат — кроп ROI
}

cv::Mat img::toNeon(const cv::Mat& src)
{
    if (src.empty())
        return src;
    cv::Mat cvim = src.clone();
    for (int i = 0; i < nBts; i++)
    {
        cv::Mat buff;
        cv::bilateralFilter(cvim, buff, 16, 16, 64);
        cvim = std::move(buff);
    }
    cv::Mat imgGray;
    cv::cvtColor(cvim, imgGray, cv::COLOR_RGB2GRAY);
    cv::Mat imgEdge;
    cv::adaptiveThreshold(imgGray, imgEdge, 255, CV_ADAPTIVE_THRESH_MEAN_C, CV_THRESH_BINARY, 5, 2);
    imgGray.release();

    cv::resize(imgEdge, imgEdge, cv::Size(cvim.size().width, cvim.size().height));
    cv::Rect myROI(0, 0, std::min(cvim.size().width, imgEdge.size().width), std::min(cvim.size().height, imgEdge.size().height));
    cvim = cvim(myROI);
    imgEdge = imgEdge(myROI);

    cv::Mat low(myROI.height, myROI.width, CV_8UC3, cv::Scalar(0, 0, 0));
    cv::bitwise_and(cvim, low, cvim, imgEdge);
    imgEdge.release();
    cvim = img::neonColors(cvim); // векторный HSV вместо попиксельного цикла
    cv::GaussianBlur(cvim, cvim, cv::Size(5,5), 2);
    return cvim;
}

cv::Mat img::toInvert(const cv::Mat& src)
{
    cv::Mat out;
    if (!src.empty())
        cv::bitwise_not(src, out);
    return out;
}

cv::Mat img::scale(const cv::Mat& src, int w, int h)
{
    cv::Mat out;
    if (!src.empty())
        cv::resize(src, out, cv::Size(w, h));
    return out;
}

cv::Mat img::warp(const cv::Mat& src, int seed, int strength)
{
    if (src.empty())
        return src;
    if (strength < 1)
        strength = 1;
    cv::RNG rng(seed >= 0 ? (uint64)(uint32_t)seed : cv::theRNG().state); // state — текущее «случайное» состояние глобального RNG
    cv::Mat img = src.clone();

    // 1) случайная перспектива: каждый угол сдвигается не более чем на strength/2 % стороны
    float f = (float)strength / 100.0f * 0.5f;
    cv::Point2f s[4] = { { 0, 0 }, { (float)img.cols, 0 },
                         { (float)img.cols, (float)img.rows }, { 0, (float)img.rows } };
    cv::Point2f d[4];
    for (int i = 0; i < 4; i++)
        d[i] = cv::Point2f(s[i].x + (float)rng.uniform(-img.cols * f, img.cols * f),
                           s[i].y + (float)rng.uniform(-img.rows * f, img.rows * f));
    cv::Mat M = cv::getPerspectiveTransform(s, d);
    cv::warpPerspective(img, img, M, img.size(), cv::INTER_LINEAR, cv::BORDER_REFLECT_101);

    // 2) «желе»: гауссово поле смещений, сглаженное размытием -> плавные волны;
    // амплитуда ~strength пикселей (поле нормировано на ~3 сигм)
    cv::Mat fx(img.rows, img.cols, CV_32FC1), fy(img.rows, img.cols, CV_32FC1);
    rng.fill(fx, cv::RNG::NORMAL, 0.0, 1.0); // OpenCV 5: константы распределения — в классе RNG
    rng.fill(fy, cv::RNG::NORMAL, 0.0, 1.0);
    double sigma = std::max(3.0, (double)std::max(img.rows, img.cols) / 60.0);
    cv::GaussianBlur(fx, fx, cv::Size(0, 0), sigma);
    cv::GaussianBlur(fy, fy, cv::Size(0, 0), sigma);
    double k = (double)strength / 3.0;
    cv::Mat mapx(img.rows, img.cols, CV_32FC1), mapy(img.rows, img.cols, CV_32FC1);
    for (int y = 0; y < img.rows; y++)
    {
        float* mx = mapx.ptr<float>(y);
        float* my = mapy.ptr<float>(y);
        const float* px = fx.ptr<float>(y);
        const float* py = fy.ptr<float>(y);
        for (int x = 0; x < img.cols; x++)
        {
            mx[x] = (float)x + (float)(px[x] * k);
            my[x] = (float)y + (float)(py[x] * k);
        }
    }
    cv::remap(img, img, mapx, mapy, cv::INTER_LINEAR, cv::BORDER_REFLECT_101);
    return img;
}
