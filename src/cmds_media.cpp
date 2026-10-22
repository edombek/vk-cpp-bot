// Медиакоманды: /vid, /apod, /pix, /vox, /crt, /neon
#include "cmds.h"
#include "img.h"
#include "str.h"
#include <cmath>
#include <cstdio>

using namespace std;

#ifndef MIN
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif // MIN

void videos(Event &eventIn, Event &eventOut, Worker *worker)
{
    if (eventIn.words.size() < 2)
        eventOut.msg += "/vid <count> <q>";
    else
    {
        string q;
        int c = str::fromString(eventIn.words[1]);
        if (c)
            q = str::summ(eventIn.words, 2);
        else
            q = str::summ(eventIn.words, 1);
        if (!c || c > 200)
            c = 200;
        json resp = eventOut.vk.send("video.search", { { "q", q }, { "count", to_string(c) }, { "adult", "1" }, { "hd", "0" }, { "filters", "mp4" } }, true);
        if (resp["response"].is_null() || resp["response"]["items"].size() == 0)
        {
            eventOut.msg = "nope";
            return;
        }
        string msg = eventOut.msg;
        eventOut.msg = msg + "держи";
        uint8_t s = 5;
        if (eventOut.type.find("wall_post_") != eventOut.type.npos)
            s = 2;
        for (uint16_t i = 0; i < resp["response"]["items"].size(); i++)
        {
            json r;
            r["video"] = resp["response"]["items"][i];
            r["type"] = "video";
            eventOut.docs.emplace_back(r);
            if(eventOut.docs.size() == s)
            {
                eventOut.send();
                eventOut.docs = {};
            }
        }
        eventOut.msg = msg + "всё)";
    }
}

void apod(Event &eventIn, Event &eventOut, Worker *worker)
{
    json resp = json::parse(eventIn.net.send("https://api.nasa.gov/planetary/apod", {{"api_key", "DEMO_KEY"}}, false));
    auto err = [&resp, &eventOut]() // ответ об сбое с дампом ответа NASA (бывшая метка err)
    {
        eventOut.msg += "что-то пошло не так, перешлите сообщение разработчику\n\n" + resp.dump(4);
    };
    if (resp["title"].is_null())
    {
        err();
        return;
    }
    eventOut.msg += resp["title"].get<string>() + ":\n\n";
    string imgurl;
    if (!resp["hdurl"].is_null())
        imgurl = resp["hdurl"];
    else if (!resp["url"].is_null())
        imgurl = resp["url"];
    else
    {
        err();
        return;
    }
    if (!resp["explanation"].is_null())
        eventOut.msg += resp["explanation"].get<string>();
    if (resp["media_type"] == "image")
    {
        string dat = eventIn.net.send(imgurl);
        Doc img;
        if (img.uploadPhoto("img.jpg", dat, eventIn.net, eventIn.vk, eventIn.peer_id))
            eventOut.docs.push_back(img);
    }
    else if (resp["media_type"] == "video")
        eventOut.msg += "\n\n" + imgurl;
    else
        err();
}

#define imgdelta 8.0
void pix(Event &eventIn, Event &eventOut, Worker *worker)
{
    bool full = eventIn.user.acess >= 2;
    for(auto doc : eventIn.docs)
    {
        img im(doc, eventIn.net);
        cv::Mat cvim = im.getMat(full);
        if(cvim.empty())
            continue;
        cv::resize(cvim, cvim, cv::Size(), 1/imgdelta, 1/imgdelta);
        cv::resize(cvim, cvim, cv::Size(), imgdelta, imgdelta, cv::INTER_NEAREST);

        if(full && im.isBig(cvim))
            eventOut.docs.push_back(img::getDoc(cvim, eventIn.peer_id, eventIn.net, eventIn.vk));
        else
            eventOut.docs.push_back(img::getPhoto(cvim, eventIn.peer_id, eventIn.net, eventIn.vk));
    }
}

#define TPAUSE 0.1 //pause in sec
#define ofset 8 //in bites

struct wav_header_t
{
    uint8_t chunkID[4]; //"RIFF" = 0x46464952
    uint32_t chunkSize; //28 [+ sizeof(wExtraFormatBytes) + wExtraFormatBytes] + sum(sizeof(chunk.id) + sizeof(chunk.size) + chunk.size)
    uint8_t format[4]; //"WAVE" = 0x45564157
    uint8_t subchunk1ID[4]; //"fmt " = 0x20746D66
    uint32_t subchunk1Size; //16 [+ sizeof(wExtraFormatBytes) + wExtraFormatBytes]
    uint16_t audioFormat;
    int16_t numChannels;
    uint32_t sampleRate;
    uint32_t byteRate;
    int16_t blockAlign;
    int16_t bitsPerSample;
    //[WORD wExtraFormatBytes;]
    //[Extra format bytes]
};
struct chunk_t
{
    uint8_t ID[4]; //"data" = 0x61746164
    uint32_t size; //Chunk data bytes
};

void vox(Event &eventIn, Event &eventOut, Worker *worker)
{
    uint32_t size = 0;
    string data;
    wav_header_t wavHeader = {};
    chunk_t wavChunk = {};
    for (size_t i = 1; i < eventIn.words.size(); i++)
    {
        bool pause = true;
        if (eventIn.words[i][0] == '-')
        {
            pause = false;
            eventIn.words[i].erase(eventIn.words[i].begin());
        }
        string path = "vox/" + eventIn.words[i] + ".wav";
        FILE* wavFile = fopen(path.c_str(), "rb");
        if (wavFile == NULL)
            continue;
        // повреждённый wav пропускаем, а не обрываемся на мусорных размерах
        if (fread(&wavHeader, sizeof(wav_header_t), 1, wavFile) != 1
            || wavHeader.subchunk1Size < 16 || wavHeader.subchunk1Size > 128)
        {
            fclose(wavFile);
            continue;
        }
        if (wavHeader.subchunk1Size > 16
            && fseek(wavFile, wavHeader.subchunk1Size - 16, SEEK_CUR) != 0)
        {
            fclose(wavFile);
            continue;
        }
        if (fread(&wavChunk, sizeof(chunk_t), 1, wavFile) != 1
            || wavChunk.size == 0 || wavChunk.size > 0x800000) // wav-фонемы заведомо меньше 8МБ
        {
            fclose(wavFile);
            continue;
        }
        vector<uint8_t> dataIn(wavChunk.size);
        if (fread(dataIn.data(), 1, wavChunk.size, wavFile) != wavChunk.size)
        {
            fclose(wavFile);
            continue;
        }
        fclose(wavFile);
        size += wavChunk.size;
        if (pause)
        {
            size += uint32_t(TPAUSE * wavHeader.byteRate);
            char lastByte = data.size() ? data[data.size() - 1] : 0;
            for (uint32_t s = 0; s < uint32_t(TPAUSE * wavHeader.byteRate); s++)
                data += lastByte;
        }
        for (uint32_t s = ofset; s + ofset < wavChunk.size; s++)
            data += dataIn[s];
    }
    if (!data.size())
    {
        eventOut.msg += "неверная комбинация vox";
        return;
    }
    size += uint32_t(TPAUSE * wavHeader.byteRate);
    char lastByte = data[data.size() - 1];
    for (uint32_t s = 0; s < uint32_t(TPAUSE * wavHeader.byteRate); s++)
        data += lastByte;
    wavChunk.size = size;
    wavHeader.subchunk1Size = 16;
    string wavDat((char*)&wavHeader, sizeof(wav_header_t));
    wavDat+=string((char*)&wavChunk, sizeof(chunk_t)) + data;
    Doc wavDoc;
    wavDoc.uploadDoc("audiomsg.wav", wavDat, eventIn.net, eventIn.vk, eventIn.peer_id, true);
    eventOut.docs.push_back(wavDoc);
}

#define nDownSampling 2
#define nBts 7
#ifndef CV_ADAPTIVE_THRESH_MEAN_C
#define CV_ADAPTIVE_THRESH_MEAN_C 0
#endif
#ifndef CV_THRESH_BINARY
#define CV_THRESH_BINARY 0
#endif
void crt(Event &eventIn, Event &eventOut, Worker *worker)
{
    bool full = eventIn.user.acess >= 2;
    for(auto doc : eventIn.docs)
    {
        img im(doc, eventIn.net);
        cv::Mat cvim = im.getMat(full);
        if(cvim.empty())
            continue;
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
        cv::Rect myROI(0, 0, MIN(cvim.size().width, imgEdge.size().width), MIN(cvim.size().height, imgEdge.size().height));
        cvim = cvim(myROI);
        imgEdge = imgEdge(myROI);
        cv::bitwise_and(imgColored, imgEdge, cvim);

        if(full && im.isBig(cvim))
            eventOut.docs.push_back(img::getDoc(cvim, eventIn.peer_id, eventIn.net, eventIn.vk));
        else
            eventOut.docs.push_back(img::getPhoto(cvim, eventIn.peer_id, eventIn.net, eventIn.vk));
    }
}

void neon(Event &eventIn, Event &eventOut, Worker *worker)
{
    bool full = eventIn.user.acess >= 2;
    for(auto doc : eventIn.docs)
    {
        img im(doc, eventIn.net);
        cv::Mat cvim = im.getMat(full);
        if(cvim.empty())
            continue;

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
        cv::Rect myROI(0, 0, MIN(cvim.size().width, imgEdge.size().width), MIN(cvim.size().height, imgEdge.size().height));
        cvim = cvim(myROI);
        imgEdge = imgEdge(myROI);

        cv::Mat low(myROI.height, myROI.width, CV_8UC3, cv::Scalar(0, 0, 0));
        cv::bitwise_and(cvim,low, cvim, imgEdge);
        imgEdge.release();
        cvim = img::neonColors(cvim); // векторный HSV вместо попиксельного цикла
        cv::GaussianBlur(cvim, cvim, cv::Size(5,5), 2);
        if(full && im.isBig(cvim))
            eventOut.docs.push_back(img::getDoc(cvim, eventIn.peer_id, eventIn.net, eventIn.vk));
        else
            eventOut.docs.push_back(img::getPhoto(cvim, eventIn.peer_id, eventIn.net, eventIn.vk));
    }
}
