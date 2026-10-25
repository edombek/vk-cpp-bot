#pragma once
#include "doc.h"
#include "events.h"
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
    // источник картинки для арт-команд: вложения события; без них — вложения
    // пересланных сообщений и контекста ответа (fwds, рекурсивно); без них — фото профиля автора
    static std::vector<Doc> sources(Event &eventIn);
    // фильтры (фаза 12 — вынесены из cmds_media, вызовы идентичны прежним циклам;
    // возвращают результат, аргумент не меняют; pix/crt/neon — как в /pix /crt /neon)
    static cv::Mat toPix(const cv::Mat& src);   // 8bit art: даунсэмпл x8 + nearest up
    static cv::Mat toCrt(const cv::Mat& src);   // cartoon: bilateral + контуры
    static cv::Mat toNeon(const cv::Mat& src);  // неон с контурами
    static cv::Mat toInvert(const cv::Mat& src);
    static cv::Mat scale(const cv::Mat& src, int w, int h);
    // случайная деформация геометрии: набор локальных «бугров», каждый тянет свою
    // область в свою сторону (между зонами картинка почти не тронута) — разные
    // участки искажаются по-разному; число зон и сила тоже случайные.
    // seed >= 0 — повторяемый результат, seed < 0 — своя энтропия на каждый вызов;
    // strength — базовая амплитуда смещений в пикселях (default 40)
    static cv::Mat warp(const cv::Mat& src, int seed = -1, int strength = 40);
    Doc doc;
    Net &net;
};
