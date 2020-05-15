#include "cmd.h"
#include "common.h"
#include "events.h"
#include "fs.h"
#include "img.h"
#include "str.h"
#include "timer.h"
#include <iostream>

#include <cmath>

#ifndef MAX
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#endif // MAX

#ifndef MIN
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif // MIN

using namespace std;

#include <stdio.h>
string getParamOfPath(string path, string p)
{
    FILE* f = fopen(path.c_str(), "r");
    char buffer[4096];
    size_t bytes_read;
    bytes_read = fread(buffer, 1, sizeof(buffer), f);
    fclose(f);
    string dat(buffer, bytes_read);
    args_t lines = str::words(dat, '\n');
    //cout << lines[0] << endl;
    for (auto line : lines)
    {
        args_t param = str::words(line, ':');
        if (param.size() < 2) continue;
        if (str::at(param[0], p))
            return str::replase(param[1], "  ", " ");
    }
    return "";
}

void stat(cmdHead)
{
    std::chrono::time_point<std::chrono::system_clock> begin, end;
    timer t;
    eventOut.net.send("http://api.vk.com");
#ifndef WIN32
    eventOut.msg += GIT_URL;
    eventOut.msg += " (" + string(GIT_VER) + ")\n";
#endif
    eventOut.msg += "Обработка VK API за: " + to_string(t.getWorked()) + "мс\n";
    eventOut.msg += "id чата (пользователь/чат): " + to_string(eventIn.from_id) + "/" + to_string(eventIn.peer_id) + "\n";
    eventOut.msg += "Работаю: " + timer::getWorkTime() + "\n";
#ifndef WIN32
    //получаем использование памяти
    string allMem = to_string((int)((float)str::fromString(getParamOfPath("/proc/meminfo", "MemTotal")) / 1024));
    string usedMem = to_string((int)((float)(str::fromString(getParamOfPath("/proc/meminfo", "MemTotal")) - str::fromString(getParamOfPath("/proc/meminfo", "MemAvailable"))) / 1024));
    string myMem = to_string((int)((float)str::fromString(getParamOfPath("/proc/self/status", "VmRSS")) / 1024));

    eventOut.msg += "CPU:" + (getParamOfPath("/proc/cpuinfo", "model name").size() ? getParamOfPath("/proc/cpuinfo", "model name") : getParamOfPath("/proc/cpuinfo", "Model")) + "\n";
    eventOut.msg += "RAM: " + usedMem + "/" + allMem + " Мб\n";
    eventOut.msg += "Я сожрал оперативы: " + myMem + " Мб\n";
    eventOut.msg += "Потоков занял: " + getParamOfPath("/proc/self/status", "Threads") + "\n\n";
#endif
    eventOut.msg += "Всего сообщений от тебя: " + std::to_string(eventOut.user.msgs) + "\n";
}

#ifdef WIN32 //винда чо ¯\_(ツ)_/¯
#define popen _popen
#define pclose _pclose
#endif
void con(cmdHead)
{
    char buffer[512];

    args_t commands = str::words(str::summ(eventIn.words, 1), '\n');
    string c = "";
    for (auto command : commands)
        c += command + " 2>&1\n";

    FILE* pipe = popen(c.c_str(), "r");
    while (fgets(buffer, sizeof buffer, pipe) != NULL)
    {
        eventOut.msg = buffer;
        eventOut.send();
        timer::sleep(1000);
    }
    eventOut.msg = "done!";
    pclose(pipe);
}

void upload(cmdHead)
{
    string filename = str::summ(eventIn.words, 1);
    string dat = fs::readData(filename);
    Doc doc;
    if (doc.uploadDoc(filename, dat, eventOut.net, eventOut.vk, eventOut.peer_id))
        eventOut.docs.push_back(doc);
}

void set(cmdHead)
{
    if (eventIn.words.size() != 3) // != <cmd, id, level>
        eventOut.msg += "/set <id> <level>...";
    else
    {
        uint32_t id = str::fromString(eventIn.words[1]);
        if (!id)
            eventOut.msg += "/set <id> <level>...";
        else
        {
            users::user user = users::getUser(id, eventOut.vk);
            user.acess = str::fromString(eventIn.words[2]);
            user.msgs--;
            users::changeUser(id, user);
            eventOut.msg += "done!";
        }
    }
}

void rename(cmdHead)
{
    if (eventIn.words.size() < 2) // != <cmd, id, level>
        eventOut.msg += "/rename <name>...";
    else
    {
        eventIn.user.name = str::summ(eventIn.words, 1);
        users::changeUser(eventIn.user.id, eventIn.user);
        eventOut.msg += "done!";
    }
}

void videos(cmdHead)
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
        string vidc = "/" + to_string(resp["response"]["items"].size());
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

#define openweathermap_msg "get token on openweathermap.org"
void weather(cmdHead)
{
    if (eventIn.words.size() < 2)
    {
        eventOut.msg += "мб город введёшь?";
        return;
    }

    //вытаскиваем токен от сервиса
    conf.lock.lock();
    json& c = conf.get();
    if (c["openweathermap_token"].is_null() || c["openweathermap_token"] == openweathermap_msg)
    {
        c["openweathermap_token"] = openweathermap_msg;
        conf.lock.unlock();
        conf.save();
        eventOut.msg += "передайте админу что он лентяй)";
        return;
    }
    string appid(c["openweathermap_token"].get<string>());
    conf.lock.unlock();

    table_t params =
    {
        { "lang", "ru" },
        { "units", "metric" },
        { "APPID", appid },
        { "q", str::summ(eventIn.words, 1) }
    };
    json weather = json::parse(eventIn.net.send("http://api.openweathermap.org/data/2.5/weather", params, false));
    if (weather["main"].is_null())
    {
        eventOut.msg += "чтота пошло не так, возможно надо ввести город транслитом или требуется вмешательство внешних сил(админа)";
        return;
    }
    string temp = "";
    temp += "погода в " + weather["sys"]["country"].get<string>() + "/" + weather["name"].get<string>() + ":";
    //temp += "\n¤" + other::getTime(weather["dt"]) + ":\n";
    temp += "\n•температура: " + to_string((int)weather["main"]["temp"]) + "°C\n•скорость ветра: " + to_string((int)weather["wind"]["speed"]) + "м/с\n•влажность: " + to_string((int)weather["main"]["humidity"]) + "%\n•описание: " + weather["weather"][0]["description"].get<string>() + "\n";
    eventOut.msg += temp;
}

void setCfg(cmdHead)
{
    if (eventIn.words.size() < 2 || eventIn.words.size() > 3)
    {
        eventOut.msg += "<param> <value> or <param>";
        return;
    }
    string param = eventIn.words[1];
    conf.lock.lock();
    if (eventIn.words.size() == 2)
    {
        if (!conf.get()[param].is_null())
            eventOut.msg += conf.get()[param].dump();
        else
            eventOut.msg += "NULL";
        conf.lock.unlock();
        return;
    }
    conf.get()[param] = json::parse(eventIn.words[2]);
    conf.lock.unlock();
    conf.save();
}

#include "game.h"
map<uint32_t, game*> gameNew = {}; // новые игры в чате
map<uint32_t, map<uint32_t, game*>> gameUsers = {}; // все игры в [chat][user]
mutex gameL;

void cleanGames(game *t, uint32_t peer_id) // удаляем игры в каталогах пользователей и саму игру
{
    gameUsers[peer_id].erase(t->users_id[0]);
    if(t->users_id[1])
        gameUsers[peer_id].erase(t->users_id[1]);
    if(!gameUsers[peer_id].size())
        gameUsers.erase(peer_id);
    delete t;
}

void gameF(cmdHead)
{
    if(!eventIn.is_chat)
    {
        eventOut.msg = "только для бесед!";
        return;
    }
    gameL.lock();
    game* t;

    if(findinmap(gameUsers, eventIn.peer_id) && findinmap(gameUsers[eventIn.peer_id], eventIn.user.id) && gameUsers[eventIn.peer_id][eventIn.user.id]->users_id[1])//игра готова?
        t = gameUsers[eventIn.peer_id][eventIn.user.id];
    else
    {
        if(findinmap(gameNew, eventIn.peer_id) && !gameNew[eventIn.peer_id]->users_id[1])//есть ли созданная в чате но без второго игрока?
        {
            if(gameNew[eventIn.peer_id]->users_id[0]==eventIn.user.id)
            {
                eventOut.msg+="нельзя создать игру с самим собой! Удаляем эту игру!";
                cleanGames(gameNew[eventIn.peer_id], eventIn.peer_id);
                gameNew.erase(eventIn.peer_id);
                gameL.unlock();
                return;
            }
            t=gameNew[eventIn.peer_id];
            t->users_id[1] = eventIn.user.id;
            gameUsers[eventIn.peer_id][eventIn.user.id] = t;
            gameNew.erase(eventIn.peer_id);
            goto printMap;
        }
        else  //создаём новую игру, ожидаем
        {
            t = new game;
            t->users_id[0] = eventIn.user.id;
            gameNew[eventIn.peer_id] = t;
            gameUsers[eventIn.peer_id][eventIn.user.id] = t;

            eventOut.msg += "игра создана, ожидаем второго игрока";
            gameL.unlock();
            return;
        }
    }

    if(eventIn.words.size() > 1 && eventIn.words[1] == "stop")
    {
        eventOut.msg+="игра остановлена!";
        cleanGames(t, eventIn.peer_id);
        gameL.unlock();
        return;
    }
    if(eventIn.user.id!=t->users_id[t->user])
    {
        eventOut.msg+="не твой ход!";
        gameL.unlock();
        return;
    }
    if(eventIn.words.size() < 3)
    {
        eventOut.msg+="кординату введи...";
        gameL.unlock();
        return;
    }

    t->gameUplevel(str::fromString(eventIn.words[1]), str::fromString(eventIn.words[2]));

printMap:
    eventOut.msg+="\n"+t->nexStep();
    eventOut.send();

    if(t->isWin()) //выйграл?
    {
        eventOut.msg="выйграл!";
        cleanGames(t, eventIn.peer_id);
        gameL.unlock();
        return;
    }

    eventOut.msg="Ходит @id" + to_string(t->users_id[t->user]);
    gameL.unlock();
}

void apod(cmdHead)
{
    json resp = json::parse(eventIn.net.send("https://api.nasa.gov/planetary/apod", {{"api_key", "DEMO_KEY"}}, false));
    string imgurl;
    Doc img;
    string dat;
    if(resp["title"].is_null())
        goto err;
    eventOut.msg+= resp["title"].get<string>()+":\n\n";
    if(resp["hdurl"].is_null())
        if(resp["url"].is_null())
            goto err;
        else
            imgurl = resp["url"];
    else
        imgurl = resp["hdurl"];
    if(!resp["explanation"].is_null())
        eventOut.msg+=resp["explanation"].get<string>();
    if(resp["media_type"]=="image")
    {
        dat = eventIn.net.send(imgurl);
        if(img.uploadPhoto("img.jpg", dat, eventIn.net, eventIn.vk, eventIn.peer_id))
            eventOut.docs.push_back(img);
    }
    else if(resp["media_type"]=="video")
        eventOut.msg+="\n\n"+imgurl;
    else
        goto err;

    return;
err:
    eventOut.msg+="что-то пошло не так, перешлите сообщение разработчику\n\n"+resp.dump(4);
    return;
}

#ifndef NO_PYTHON
#include "py.h"
#endif // NO_PYTHON
void pycmd(cmdHead)
{
#ifndef NO_PYTHON
    std::string str(str::summ(eventIn.words, 1));
    pyF interpreter(eventOut);
    str = interpreter.exec(str);
    if(str.size())
        eventOut.msg = str;
    else
        eventOut.msg = "done!";
#else
    eventOut.msg = "не собрано";
#endif
}

#define imgdelta 8.0
#define imgcolorsinch 8
#define imgcolorscoff 255/imgcolorsinch
#define imgcolorcorr(c) round(round((float(c)/255)*imgcolorsinch)*imgcolorscoff)
void pix(cmdHead)
{
    bool full = eventIn.user.acess >= 2;
    for(auto doc : eventIn.docs)
    {
        img im(doc, eventIn.net, full);
        if(im.im.empty())
            continue;
        cv::resize(im.im, im.im, cv::Size(), 1/imgdelta, 1/imgdelta);
        cv::resize(im.im, im.im, cv::Size(), imgdelta, imgdelta, cv::INTER_NEAREST);

        if(full && im.isBig())
            eventOut.docs.push_back(im.getDoc(eventIn.peer_id, eventIn.net, eventIn.vk));
        else
            eventOut.docs.push_back(im.getPhoto(eventIn.peer_id, eventIn.net, eventIn.vk));
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

void vox(cmdHead)
{
    unsigned long size = 0;
    string data;
    wav_header_t wavHeader;
    chunk_t wavChunk;
    for (int i = 1; i < eventIn.words.size(); i++)
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
        fread(&wavHeader, sizeof(wav_header_t), 1, wavFile);
        uint8_t* offset = new uint8_t[wavHeader.subchunk1Size - 16];
        fread(offset, wavHeader.subchunk1Size - 16, 1, wavFile);
        delete[] offset;
        fread(&wavChunk, sizeof(chunk_t), 1, wavFile);
        uint8_t* dataIn = new uint8_t[wavChunk.size];
        fread(dataIn, wavChunk.size, 1, wavFile);
        fclose(wavFile);
        size += wavChunk.size;
        if (pause)
        {
            size += TPAUSE * wavHeader.byteRate;
            for (int s = 0; s < TPAUSE * wavHeader.byteRate; s++)
                data += data[data.size() - 1];
        }
        for (int s = ofset; s < wavChunk.size - ofset; s++)
            data += dataIn[s];
        delete[] dataIn;
    }
    if (!data.size())
    {
        eventOut.msg += "неверная комбинация vox";
        return;
    }
    size += TPAUSE * wavHeader.byteRate;
    for (int s = 0; s < TPAUSE * wavHeader.byteRate; s++)
        data += data[data.size() - 1];
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
void crt(cmdHead)
{
    bool full = eventIn.user.acess >= 2;
    for(auto doc : eventIn.docs)
    {
        img im(doc, eventIn.net, full);
        if(im.im.empty())
            continue;

        cv::Mat cvim = im.im;
        cv::Mat imgColored = cvim.clone();
        for (int i = 0; i < nDownSampling; i++)
            cv::pyrDown(imgColored, imgColored);
        for (int i = 0; i < nBts; i++)
        {
            cv::Mat buff;
            cv::bilateralFilter(imgColored, buff, 9, 9, 7);
            imgColored = buff.clone();
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

        if(full && im.isBig())
            eventOut.docs.push_back(im.getDoc(eventIn.peer_id, eventIn.net, eventIn.vk));
        else
            eventOut.docs.push_back(im.getPhoto(eventIn.peer_id, eventIn.net, eventIn.vk));
    }
}

/*#define lineRadius 12
void line(cmdHead)
{
    for(auto doc : eventIn.docs)
    {
        img im(doc, eventIn.net);
        if(!im.im)
            continue;
        gdImageGrayScale(im.im);
        img blured(gdImageCopyGaussianBlurred(im.im, lineRadius/2, -1.0));
        img lined(blured.im->sx, blured.im->sy);

        for(int y = 0; y < blured.im->sy; y+=lineRadius)
            for(int x = 0; x < blured.im->sx; x++)
            {
                int i = floor(gdTrueColorGetRed(gdImageGetPixel(blured.im, x, y)) / 255.0 * lineRadius);
                gdImageSetPixel(lined.im, x, y+lineRadius-i, 0xFFFFFF);
            }
        for(int y = 0; y < blured.im->sy; y++)
            for(int x = 0; x < blured.im->sx; x+=lineRadius)
            {
                int i = floor(gdTrueColorGetRed(gdImageGetPixel(blured.im, x, y)) / 255.0 * lineRadius);
                gdImageSetPixel(lined.im, x+lineRadius-i, y, 0xFFFFFF);
            }

        eventOut.docs.push_back(lined.getPhoto(eventIn.peer_id, eventIn.net, eventIn.vk));
    }
}*/

#include <algorithm>
string getNumber() // генерация случайного четырёхзначного числа без повторений цифр
{
    string nums = "1234567890";
    srand(time(NULL));
    random_shuffle(nums.begin(), nums.end());
    if(nums[0]=='0')
        nums[0]=nums[9];
    nums.resize(4);
    return nums;
}

map<int, string> randNumbers = {}; // все загаданные числа для каждого пользователя
mutex bcL;
void bc(cmdHead)
{
    bcL.lock();

    goto _pass;
pass:
    bcL.unlock();
    return;
_pass:

    if(!findinmap(randNumbers, eventIn.user.id))//если игра не создана
    {
        randNumbers[eventIn.user.id] = getNumber();
        eventOut.msg+="\
цель данной игры отгадать загаданное компьютером число.\n\
Даются подсказки в виде двух значений.\n\
Коров -- числа совпадений цифр без совпадений их положения.\n\
Быков -- числа точных совпадений цифр.\n\
Ответ боту даётся как аргумент к этой команде.";
        goto pass;
    }
    if(eventIn.words.size() < 2)
    {
        eventOut.msg+="ты не указал четырёхзначное число.";
        goto pass;
    }
    string userNumber = eventIn.words[1];
    if(!(userNumber.size() == 4 && str::fromString(userNumber) && userNumber[0] != '0')) // если это не четырёх значное число
    {
        eventOut.msg+="ты указал не четырёхзначное число.";
        goto pass;
    }


    if(userNumber == randNumbers[eventIn.user.id])
    {
        eventOut.msg+="молодец!!! Ты отгадал число!";
        randNumbers.erase(eventIn.user.id);
        goto pass;
    }

    string randNumber = randNumbers[eventIn.user.id];

    uint8_t c = 0; // коровы
    uint8_t b = 0; // быки
    for(uint8_t ri = 0; ri < 4; ri++)
    {
        if(randNumber[ri] == userNumber[ri])// если бык
        {
            b++;
            continue;
        }
        for(uint8_t ui = 0; ui < 4; ui++)
            if(randNumber[ri] == userNumber[ui]) // если корова
            {
                c++;
                break;
            }
    }

    eventOut.msg = "найдено:\n  Коров: " + to_string(c) + "\n  Быков: " + to_string(b);
    bcL.unlock();
}

void neon(cmdHead)
{
    bool full = eventIn.user.acess >= 2;
    for(auto doc : eventIn.docs)
    {
        img im(doc, eventIn.net, full);
        if(im.im.empty())
            continue;

        for (int i = 0; i < nBts; i++)
        {
            cv::Mat buff;
            cv::bilateralFilter(im.im, buff, 16, 16, 64);
            im.im = buff.clone();
        }
        cv::Mat imgGray;
        cv::cvtColor(im.im, imgGray, cv::COLOR_RGB2GRAY);
        cv::Mat imgEdge;
        cv::adaptiveThreshold(imgGray, imgEdge, 255, CV_ADAPTIVE_THRESH_MEAN_C, CV_THRESH_BINARY, 5, 2);
        imgGray.release();

        cv::resize(imgEdge, imgEdge, cv::Size(im.im.size().width, im.im.size().height));
        cv::Rect myROI(0, 0, MIN(im.im.size().width, imgEdge.size().width), MIN(im.im.size().height, imgEdge.size().height));
        im.im = im.im(myROI);
        imgEdge = imgEdge(myROI);

        cv::Mat low(myROI.height, myROI.width, CV_8UC3, cv::Scalar(0, 0, 0));
        cv::bitwise_and(im.im,low, im.im, imgEdge);
        imgEdge.release();
        for(unsigned int i = 0; i < im.im.rows; i++)
            for(unsigned int j = 0; j < im.im.cols; j++)
            {
                cv::Vec3b &bgrPixel = im.im.at<cv::Vec3b>(i, j);
                if (bgrPixel[0] + bgrPixel[1] + bgrPixel[2] == 0) continue;
                hsv_t cHsv = rgb2hsv({bgrPixel[2]/255.0, bgrPixel[1]/255.0, bgrPixel[0]/255.0});
                //cHsv.s = 1;
                cHsv.v = 1;//pow(cHsv.v, 0.5);
                rgb_t cRgb = hsv2rgb(cHsv);
                bgrPixel[0] = cRgb.b * 255;
                bgrPixel[1] = cRgb.g * 255;
                bgrPixel[2] = cRgb.r * 255;
            }
        cv::GaussianBlur(im.im, im.im, cv::Size(5,5), 2);
        if(full && im.isBig())
            eventOut.docs.push_back(im.getDoc(eventIn.peer_id, eventIn.net, eventIn.vk));
        else
            eventOut.docs.push_back(im.getPhoto(eventIn.peer_id, eventIn.net, eventIn.vk));
    }
}
