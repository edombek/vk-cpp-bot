#include "img.h"
#include "vk.h"
#include <algorithm>

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
    const int minDim = std::min(img.rows, img.cols);

    // набор локальных «бугров»: каждый тянет свою область в свою сторону,
    // области между ними остаются нетронутыми -> разные участки искажаются по-разному
    int places = std::min(24, std::max(4, minDim / 40)); // ~12 зон для картинки ~500 px
    cv::Mat fx(img.rows, img.cols, CV_32FC1), fy(img.rows, img.cols, CV_32FC1);
    fx = cv::Scalar(0);
    fy = cv::Scalar(0);
    for (int i = 0; i < places; i++)
    {
        float cx = (float)rng.uniform(0.05, 0.95) * img.cols;
        float cy = (float)rng.uniform(0.05, 0.95) * img.rows;
        float r = (float)rng.uniform(0.10, 0.30) * minDim;  // радиус зоны
        float a = (float)rng.uniform(0.2, 1.0) * strength;  // амплитуда своя у каждой зоны
        double ang = rng.uniform(0.0, 2.0 * CV_PI);         // и своё направление
        float dx = (float)cos(ang) * a, dy = (float)sin(ang) * a;
        int x0 = std::max(0, (int)(cx - r)), x1 = std::min(img.cols - 1, (int)(cx + r));
        int y0 = std::max(0, (int)(cy - r)), y1 = std::min(img.rows - 1, (int)(cy + r));
        for (int y = y0; y <= y1; y++)
        {
            float* mx = fx.ptr<float>(y);
            float* my = fy.ptr<float>(y);
            float ddy = (float)(y - cy) * (y - cy);
            for (int x = x0; x <= x1; x++)
            {
                float ddx = (float)(x - cx) * (x - cx);
                float d2 = ddx + ddy;
                if (d2 >= r * r)
                    continue;
                float w = 1.0f - d2 / (r * r); // плавный спад к краю зоны
                w = w * w;
                mx[x] += dx * w;
                my[x] += dy * w;
            }
        }
    }
    cv::Mat mapx(img.rows, img.cols, CV_32FC1), mapy(img.rows, img.cols, CV_32FC1);
    for (int y = 0; y < img.rows; y++)
    {
        float* mx = mapx.ptr<float>(y);
        float* my = mapy.ptr<float>(y);
        const float* px = fx.ptr<float>(y);
        const float* py = fy.ptr<float>(y);
        for (int x = 0; x < img.cols; x++)
        {
            mx[x] = (float)x + px[x];
            my[x] = (float)y + py[x];
        }
    }
    cv::remap(img, img, mapx, mapy, cv::INTER_LINEAR, cv::BORDER_REFLECT_101);
    return img;
}
