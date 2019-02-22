#ifdef __linux__
#include <curl/curl.h>
#elif _WIN32
#pragma comment(lib, "libcurl.lib")
#include "win32deps/include/curl/curl.h"
#endif
#include "net.h"
#include <iostream>
using namespace std;

#define net_agent "EVGESHAd vk-cpp-bot"
#ifdef DEBUG
#define printOut
#endif // DEBUG

size_t writer(char* data, size_t size, size_t nmemb, string* buffer)
{
    int result = 0;
    if (buffer != NULL) {
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
    curl_global_init(CURL_GLOBAL_ALL);
}

string Net::urlEncode(string str)
{
    char* esc_text = curl_easy_escape(NULL, str.c_str(), str.length());
    string result = esc_text;

    curl_free(esc_text);
    return result;
}

void Net::send(string url, table_t param, bool post)
{
    if (!param.size())
        return this->send(url);
    string paramline = "";
    for (auto iter = param.begin(); iter != param.end(); iter++) {
        paramline += iter->first + "=" + urlEncode(iter->second) + "&";
    }
    if (post)
        return this->send(url, paramline);
    return this->send(url + "?" + paramline);
}

void Net::send(string url, string params)
{
    this->buffer = "";
    if (this->curl) {
        curl_easy_setopt(this->curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(this->curl, CURLOPT_WRITEDATA, &this->buffer);
        curl_easy_setopt(this->curl, CURLOPT_USERAGENT, net_agent);
        curl_easy_setopt(this->curl, CURLOPT_WRITEFUNCTION, writer);
        curl_easy_setopt(this->curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(this->curl, CURLOPT_SSL_VERIFYHOST, 0L);
        curl_easy_setopt(this->curl, CURLOPT_TIMEOUT, 600L);
        if (params != "") {
            curl_easy_setopt(curl, CURLOPT_POST, 1);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, params.c_str());
        }
        this->result = curl_easy_perform(curl);
        if (this->result != CURLE_OK)
            cout << "CURL ERROR: " << curl_easy_strerror(this->result) << endl;
#ifdef printOut
        cout << endl
             << url << "(" << params << "(" << endl
             << "	" << buffer << endl;
#endif
    }
}
