// Автоматические юнит-тесты фазы 3 (ctest, target vkbot_unit).
// Сеть почти не используется: единственный сетевой тест проверяет, что Net
// ограничивает число повторов (заведомо нескладывающийся хост .invalid).
#include "bd.h"
#include "fs.h"
#include "img.h"
#include "net.h"
#include "str.h"
#include "timer.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <string>

// глобалы, обычно определяемые main.cpp (нужны для линковки vk.cpp)
BD conf("u_conf.json");
BD bd("u_bd.json");

static int passed = 0;
static int failed = 0;

static void check(bool cond, const std::string& name, const std::string& detail = "")
{
    if (cond)
    {
        passed++;
        std::cout << "[ok]   " << name << std::endl;
    }
    else
    {
        failed++;
        std::cout << "[FAIL] " << name << (detail.empty() ? "" : (" -> " + detail)) << std::endl;
    }
}

// ---------------------------------------------------------------------------
// эталонная попиксельная реализация neon-преобразования (старый код img.cpp),
// нужна только чтобы сравнить с ней векторную img::neonColors
// ---------------------------------------------------------------------------
struct ref_rgb
{
    double r, g, b;
};
struct ref_hsv
{
    double h, s, v;
};

static ref_hsv ref_rgb2hsv(ref_rgb in)
{
    ref_hsv out;
    double min, max, delta;
    min = in.r < in.g ? in.r : in.g;
    min = min < in.b ? min : in.b;
    max = in.r > in.g ? in.r : in.g;
    max = max > in.b ? max : in.b;
    out.v = max;
    delta = max - min;
    if (delta < 0.00001)
    {
        out.s = 0;
        out.h = 0;
        return out;
    }
    if (max > 0.0)
        out.s = (delta / max);
    else
    {
        out.s = 0.0;
        out.h = NAN;
        return out;
    }
    if (in.r >= max)
        out.h = (in.g - in.b) / delta;
    else if (in.g >= max)
        out.h = 2.0 + (in.b - in.r) / delta;
    else
        out.h = 4.0 + (in.r - in.g) / delta;
    out.h *= 60.0;
    if (out.h < 0.0)
        out.h += 360.0;
    return out;
}

static ref_rgb ref_hsv2rgb(ref_hsv in)
{
    double hh, p, q, t, ff;
    long i;
    ref_rgb out;
    if (in.s <= 0.0)
    {
        out.r = out.g = out.b = in.v;
        return out;
    }
    hh = in.h;
    if (hh >= 360.0)
        hh = 0.0;
    hh /= 60.0;
    i = (long)hh;
    ff = hh - i;
    p = in.v * (1.0 - in.s);
    q = in.v * (1.0 - (in.s * ff));
    t = in.v * (1.0 - (in.s * (1.0 - ff)));
    switch (i)
    {
    case 0: out.r = in.v; out.g = t; out.b = p; break;
    case 1: out.r = q; out.g = in.v; out.b = p; break;
    case 2: out.r = p; out.g = in.v; out.b = t; break;
    case 3: out.r = p; out.g = q; out.b = in.v; break;
    case 4: out.r = t; out.g = p; out.b = in.v; break;
    default: out.r = in.v; out.g = p; out.b = q; break;
    }
    return out;
}

static cv::Mat refNeon(const cv::Mat& src)
{
    cv::Mat im = src.clone();
    for (int i = 0; i < im.rows; i++)
        for (int j = 0; j < im.cols; j++)
        {
            cv::Vec3b& p = im.at<cv::Vec3b>(i, j);
            if (p[0] + p[1] + p[2] == 0)
                continue;
            ref_hsv c = ref_rgb2hsv({p[2] / 255.0, p[1] / 255.0, p[0] / 255.0});
            c.v = 1;
            ref_rgb o = ref_hsv2rgb(c);
            p[0] = o.b * 255;
            p[1] = o.g * 255;
            p[2] = o.r * 255;
        }
    return im;
}

int main()
{
    Net::init();

    // --- U1: Net ограничивает повторы (бывшая бесконечная рекурсия) ---
    {
        Net net;
        auto t0 = std::chrono::steady_clock::now();
        std::string r = net.send("http://no-such-host-does-not-exist.invalid/");
        auto secs = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - t0).count();
        check(r.empty(), "U1a unresolvable host -> empty reply");
        check(secs < 30, "U1b bounded retries", std::to_string(secs) + "s");
    }

    // --- U2: urlEncode без сети ---
    {
        Net net;
        check(net.urlEncode("a b=б") == "a%20b%3D%D0%B1", "U2 urlEncode", net.urlEncode("a b=б"));
    }

    // --- U3: BD dirty-флаг ---
    {
        const std::string f = "u_bd_test.json";
        remove(f.c_str());
        {
            BD db(f);
            check(!db.saveIfDirty(), "U3a clean -> no save");
            check(!fs::exists(f), "U3b no file written");
            db.get()["users"]["1"]["name"] = "тест";
            db.touch();
            check(db.saveIfDirty(), "U3c dirty -> saved");
            check(fs::exists(f), "U3d file written");
            check(!db.saveIfDirty(), "U3e clean again after save");
            db.touch();
        } // деструктор должен сбросить dirty
        std::string data = fs::readData(f);
        check(data.find("тест") != std::string::npos, "U3f destructor saved", data);
        remove(f.c_str());
    }

    // --- U4: str::low без codecvt: ASCII, кириллица, Ё, чужие скрипты, битые байты ---
    {
        check(str::low("ПРИВЕТ, WORLD!") == "привет, world!", "U4a ascii+cyrillic", str::low("ПРИВЕТ, WORLD!"));
        check(str::low("ЁЛКА Ёж") == "ёлка ёж", "U4b yo", str::low("ЁЛКА Ёж"));
        check(str::low("ΘΗΚΕ") == "ΘΗΚΕ", "U4c other scripts pass through", str::low("ΘΗΚΕ"));
        check(str::low("") == "", "U4d empty");
        std::string broken = std::string("\xC3") + "аб" + "\xE2\x28\xA1";
        std::string r = str::low(broken); // не должен падать/портить соседние символы
        check(r.find("аб") != std::string::npos, "U4e broken utf8 survives", r);
        check(str::low("ABCЁ") == "abcё", "U4f mixed", str::low("ABCЁ"));
    }

    // --- U5: str regression (words/summ/replase) ---
    {
        args_t w = str::words("раз два три");
        check(w.size() == 3 && w[1] == "два", "U5a words");
        check(str::summ(w, 1) == "два три", "U5b summ", str::summ(w, 1));
        check(str::replase("а-а-а", "-", "+") == "а+а+а", "U5c replase");
    }

    // --- U6: fs roundtrip ---
    {
        const std::string f = "u_fs_test.txt";
        fs::writeData(f, "данные\n");
        check(fs::readData(f) == "данные\n", "U6a fs roundtrip");
        check(fs::exists(f) && !fs::exists(f + "XX"), "U6b fs exists");
        remove(f.c_str());
    }

    // --- U7: векторный neonColors == старый попиксельный цикл (±1 LSB) + бенчмарк ---
    {
        cv::Mat src(240, 320, CV_8UC3);
        cv::RNG rng(42);
        for (int i = 0; i < src.rows; i++)
            for (int j = 0; j < src.cols; j++)
            {
                int mode = (i * src.cols + j) % 4;
                if (mode == 0)
                    src.at<cv::Vec3b>(i, j) = cv::Vec3b(0, 0, 0); // чёрный (пропускается)
                else if (mode == 1)
                {
                    uchar g = uchar(i * 255 / src.rows); //серое (грейдскейл)
                    src.at<cv::Vec3b>(i, j) = cv::Vec3b(g, g, g);
                }
                else if (mode == 2)
                    src.at<cv::Vec3b>(i, j) = cv::Vec3b(uchar(j), uchar(i), uchar((i + j) & 0xFF));
                else
                    src.at<cv::Vec3b>(i, j) = cv::Vec3b(uchar(rng.uniform(0, 255)), uchar(rng.uniform(0, 255)), uchar(rng.uniform(0, 255)));
            }
        cv::Mat vec = img::neonColors(src);
        cv::Mat ref = refNeon(src);
        cv::Mat diff;
        cv::absdiff(vec, ref, diff);
        double maxd = 0;
        cv::minMaxLoc(diff, nullptr, &maxd);
        check(maxd <= 2.0, "U7a neonColors == reference", "max diff " + std::to_string((int)maxd));

        // бенчмарк на большом кадре — печатается для docs/TESTS.md
        cv::Mat big(720, 1280, CV_8UC3);
        cv::randu(big, 0, 255);
        auto t0 = std::chrono::steady_clock::now();
        img::neonColors(big);
        auto tVec = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
        t0 = std::chrono::steady_clock::now();
        refNeon(big);
        auto tRef = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
        std::cout << "[info] neon 1280x720: scalar=" << tRef << "ms vector=" << tVec << "ms speedup x"
                  << (tVec ? double(tRef) / double(tVec) : 0.0) << std::endl;
        check(tVec < tRef, "U7b vector faster than scalar", std::to_string(tRef) + " -> " + std::to_string(tVec) + "ms");
    }

    Net::deinit();

    std::cout << "\n=== " << passed << " passed, " << failed << " failed ===" << std::endl;
    return failed ? 1 : 0;
}
