// Медиакоманды: /vid, /pix, /vox, /crt, /neon (семейство /apod — в cmds_apod.cpp)
#include "cmds.h"
#include "img.h"
#include "str.h"
#include <cmath>
#include <cstdio>

using namespace std;

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
        if (eventOut.type.find("wall_post_") != eventOut.type.npos || eventOut.type.find("wall_reply_") != eventOut.type.npos)
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

// Источник картинки для арт-команд — img::sources (вложения события, иначе фото профиля)

void pix(Event &eventIn, Event &eventOut, Worker *worker)
{
    bool full = eventIn.user.acess >= 2;
    for(auto doc : img::sources(eventIn))
    {
        img im(doc, eventIn.net);
        cv::Mat cvim = im.getMat(full);
        if(cvim.empty())
            continue;
        cvim = img::toPix(cvim);

        Doc art = (full && im.isBig(cvim))
                      ? img::getDoc(cvim, eventIn.peer_id, eventIn.net, eventIn.vk)
                      : img::getPhoto(cvim, eventIn.peer_id, eventIn.net, eventIn.vk);
        if (!art.type.empty())
            eventOut.docs.push_back(art);
        else if (eventOut.docs.empty())
            // пустой Doc из getPhoto/getDoc = загрузка не удалась (для group-токена стена фото
            // не принимает: getMessagesUploadServer 901 на сообщество, getWallUploadServer 27) —
            // объясняем вместо молчания
            eventOut.msg += (eventIn.type.find("wall_") != std::string::npos)
                ? "картинку приложить не удалось (VK ограничивает загрузку фото от имени сообщества в стену) — пришли команду в личные сообщения"
                : "не удалось загрузить картинку (сбой upload-сервера) — попробуй ещё раз";
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

void crt(Event &eventIn, Event &eventOut, Worker *worker)
{
    bool full = eventIn.user.acess >= 2;
    for(auto doc : img::sources(eventIn))
    {
        img im(doc, eventIn.net);
        cv::Mat cvim = im.getMat(full);
        if(cvim.empty())
            continue;
        cvim = img::toCrt(cvim);

        Doc art = (full && im.isBig(cvim))
                      ? img::getDoc(cvim, eventIn.peer_id, eventIn.net, eventIn.vk)
                      : img::getPhoto(cvim, eventIn.peer_id, eventIn.net, eventIn.vk);
        if (!art.type.empty())
            eventOut.docs.push_back(art);
        else if (eventOut.docs.empty())
            // пустой Doc из getPhoto/getDoc = загрузка не удалась (для group-токена стена фото
            // не принимает: getMessagesUploadServer 901 на сообщество, getWallUploadServer 27) —
            // объясняем вместо молчания
            eventOut.msg += (eventIn.type.find("wall_") != std::string::npos)
                ? "картинку приложить не удалось (VK ограничивает загрузку фото от имени сообщества в стену) — пришли команду в личные сообщения"
                : "не удалось загрузить картинку (сбой upload-сервера) — попробуй ещё раз";
    }
}

void neon(Event &eventIn, Event &eventOut, Worker *worker)
{
    bool full = eventIn.user.acess >= 2;
    for(auto doc : img::sources(eventIn))
    {
        img im(doc, eventIn.net);
        cv::Mat cvim = im.getMat(full);
        if(cvim.empty())
            continue;
        cvim = img::toNeon(cvim);
        Doc art = (full && im.isBig(cvim))
                      ? img::getDoc(cvim, eventIn.peer_id, eventIn.net, eventIn.vk)
                      : img::getPhoto(cvim, eventIn.peer_id, eventIn.net, eventIn.vk);
        if (!art.type.empty())
            eventOut.docs.push_back(art);
        else if (eventOut.docs.empty())
            // пустой Doc из getPhoto/getDoc = загрузка не удалась (для group-токена стена фото
            // не принимает: getMessagesUploadServer 901 на сообщество, getWallUploadServer 27) —
            // объясняем вместо молчания
            eventOut.msg += (eventIn.type.find("wall_") != std::string::npos)
                ? "картинку приложить не удалось (VK ограничивает загрузку фото от имени сообщества в стену) — пришли команду в личные сообщения"
                : "не удалось загрузить картинку (сбой upload-сервера) — попробуй ещё раз";
    }
}
