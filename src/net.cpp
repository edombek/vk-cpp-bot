#include "net.h"
#include "timer.h"
#include <cstdio>
#include <iostream>
using namespace std;

#ifdef WIN32
#pragma comment(lib, "libcurl.lib")
#endif

#define net_agent "EVGESHAd vk-cpp-bot"
//#define printOut

// лимит повторов при сетевых сбоях (раньше была бесконечная рекурсия)
static const int netRetries = 5;

size_t writer(char* data, size_t size, size_t nmemb, string* buffer)
{
    int result = 0;
    if (buffer != NULL)
    {
        buffer->append(data, size * nmemb);
        result = size * nmemb;
    }
    return result;
}

Net::Net()
{
    this->curl = curl_easy_init();
}

Net::~Net()
{
    curl_easy_cleanup(this->curl);
}

void Net::init()
{
    curl_global_init(CURL_GLOBAL_DEFAULT);
}

void Net::deinit()
{
    curl_global_cleanup();
}

string Net::urlEncode(const string& str)
{
    char* esc_text = curl_easy_escape(NULL, str.c_str(), str.length());
    string result = esc_text;

    curl_free(esc_text);
    return result;
}

void Net::setTimeout(long seconds)
{
    if (seconds < 1)
        seconds = 1;
    this->timeoutSec = seconds;
}

string Net::send(const string& url, const table_t& param, bool post)
{
    if (!param.size())
        return this->send(url);
    string paramline;
    for (auto iter = param.begin(); iter != param.end(); iter++)
    {
        paramline += iter->first + "=" + urlEncode(iter->second) + "&";
    }
    if (post)
        return this->send(url, paramline);
    return this->send(url + "?" + paramline);
}

string Net::send(const string& url, const string& params)
{
    string buffer;
    if (!this->curl)
        return buffer;
    for (int attempt = 0;; attempt++)
    {
        curl_easy_setopt(this->curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(this->curl, CURLOPT_WRITEDATA, &buffer);
        curl_easy_setopt(this->curl, CURLOPT_USERAGENT, net_agent);
        curl_easy_setopt(this->curl, CURLOPT_WRITEFUNCTION, writer);
        curl_easy_setopt(this->curl, CURLOPT_TIMEOUT, this->timeoutSec);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        if (params != "")
        {
            curl_easy_setopt(curl, CURLOPT_POST, 1L);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, params.c_str());
        }
        this->result = curl_easy_perform(curl);
        if (this->result != CURLE_OK)
            cout << "CURL ERROR(" << this->result << "): " << curl_easy_strerror(this->result) << endl;
        if ((this->result == CURLE_COULDNT_RESOLVE_HOST || this->result == CURLE_OPERATION_TIMEDOUT) && attempt < netRetries)
        {
            cout << "wait internet connection..." << endl;
            timer::sleep(1000);
            curl_easy_reset(this->curl);
            continue; // повтор вместо рекурсии; после лимита вернём пустой ответ
        }
        break;
    }
#ifdef printOut
    cout << endl
         << url << "(" << params << ")" << endl
         << "	" << buffer << endl;
#endif
    curl_easy_reset(this->curl);
    return buffer;
}

string Net::upload(const string& url, const string& filename, const string& data, const string& field)
{
    string buffer;
    if (!this->curl)
        return buffer;
    for (int attempt = 0;; attempt++)
    {
        // mime пересоздаётся на каждую попытку: curl_easy_reset снимает опции
        curl_mime* mime = curl_mime_init(this->curl);
        if (mime)
        {
            curl_mimepart* part = curl_mime_addpart(mime);
            curl_mime_name(part, field.c_str()); // «file» для docs/getMessagesUploadServer, «photo» для photos/…
            curl_mime_filename(part, filename.c_str());
            curl_mime_data(part, data.c_str(), data.size());
            curl_easy_setopt(this->curl, CURLOPT_MIMEPOST, mime);
        }

        curl_easy_setopt(this->curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(this->curl, CURLOPT_WRITEDATA, &buffer);
        curl_easy_setopt(this->curl, CURLOPT_USERAGENT, net_agent);
        curl_easy_setopt(this->curl, CURLOPT_WRITEFUNCTION, writer);
        curl_easy_setopt(this->curl, CURLOPT_TIMEOUT, this->timeoutSec);
        this->result = curl_easy_perform(curl);
        if (this->result != CURLE_OK)
            cout << "CURL ERROR(" << this->result << "): " << curl_easy_strerror(this->result) << endl;
        bool retry = (this->result == CURLE_COULDNT_RESOLVE_HOST || this->result == CURLE_OPERATION_TIMEDOUT) && attempt < netRetries;
#ifdef printOut
        cout << endl
             << url << endl
             << "	" << buffer << endl;
#endif
        if (mime)
            curl_mime_free(mime);
        if (retry)
        {
            cout << "wait internet connection..." << endl;
            timer::sleep(1000);
            curl_easy_reset(this->curl);
            continue; // повтор вместо рекурсии; после лимита вернём пустой ответ
        }
        break;
    }
    curl_easy_reset(this->curl);
    return buffer;
}
