#include "img.h"
#include "vk.h"

#ifdef WIN32
#pragma comment(lib, "opencv_core2413.lib")
#pragma comment(lib, "opencv_imgproc2413.lib")
#pragma comment(lib, "opencv_highgui2413.lib")
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
