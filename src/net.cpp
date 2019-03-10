#ifdef __linux__
#include <curl/curl.h>
#elif _WIN32
#pragma comment(lib, "libcurl.lib")
#include "win32deps/include/curl/curl.h"
#endif
#include "net.h"
#include <cstdio>
#include <iostream>
using namespace std;

#define net_agent "EVGESHAd vk-cpp-bot"
#ifdef DEBUG
#define printOut
#endif // DEBUG

#if LIBCURL_VERSION_NUM >= 0x073d00
/* In libcurl 7.61.0, support was added for extracting the time in plain
   microseconds. Older libcurl versions are stuck in using 'double' for this
   information so we complicate this example a bit by supporting either
   approach. */
#define TIME_IN_US 1 /* microseconds */
#define TIMETYPE curl_off_t
#define TIMEOPT CURLINFO_TOTAL_TIME_T
#define MINIMAL_PROGRESS_FUNCTIONALITY_INTERVAL 3000000
#else
#define TIMETYPE double
#define TIMEOPT CURLINFO_TOTAL_TIME
#define MINIMAL_PROGRESS_FUNCTIONALITY_INTERVAL 3
#endif

#define STOP_DOWNLOAD_AFTER_THIS_MANY_BYTES 6000

struct myprogress {
    TIMETYPE lastruntime; /* type depends on version, see above */
    CURL* curl;
};

/* this is how the CURLOPT_XFERINFOFUNCTION callback works */
static int xferinfo(void* p,
    curl_off_t dltotal, curl_off_t dlnow,
    curl_off_t ultotal, curl_off_t ulnow)
{
    struct myprogress* myp = (struct myprogress*)p;
    CURL* curl = myp->curl;
    TIMETYPE curtime = 0;

    curl_easy_getinfo(curl, TIMEOPT, &curtime);

    /* under certain circumstances it may be desirable for certain functionality
     to only run every N seconds, in order to do this the transaction time can
     be used */
    if ((curtime - myp->lastruntime) >= MINIMAL_PROGRESS_FUNCTIONALITY_INTERVAL)
        myp->lastruntime = curtime;

    fprintf(stderr, "\rUP: %" CURL_FORMAT_CURL_OFF_T " of %" CURL_FORMAT_CURL_OFF_T "  DOWN: %" CURL_FORMAT_CURL_OFF_T " of %" CURL_FORMAT_CURL_OFF_T,
        ulnow, ultotal, dlnow, dltotal);

    if (dlnow > STOP_DOWNLOAD_AFTER_THIS_MANY_BYTES)
        return 1;
    return 0;
}

#if LIBCURL_VERSION_NUM < 0x072000
/* for libcurl older than 7.32.0 (CURLOPT_PROGRESSFUNCTION) */
static int older_progress(void* p,
    double dltotal, double dlnow,
    double ultotal, double ulnow)
{
    return xferinfo(p,
        (curl_off_t)dltotal,
        (curl_off_t)dlnow,
        (curl_off_t)ultotal,
        (curl_off_t)ulnow);
}
#endif

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
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        if (params != "") {
            curl_easy_setopt(curl, CURLOPT_POST, 1L);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, params.c_str());
        }
        this->result = curl_easy_perform(curl);
        if (this->result != CURLE_OK)
            cout << "CURL ERROR: " << curl_easy_strerror(this->result) << endl;
#ifdef printOut
        cout << endl
             << url << "(" << params << ")" << endl
             << "	" << buffer << endl;
#endif
        curl_easy_reset(this->curl);
    }
}

void Net::upload(string url, string filename, string& data)
{
    this->buffer = "";
    struct myprogress prog;
    if (this->curl) {
        prog.lastruntime = 0;
        prog.curl = this->curl;
        curl_mime* mime = curl_mime_init(this->curl);
        if (mime) {
            curl_mimepart* part = curl_mime_addpart(mime);
            curl_mime_name(part, "file");
            curl_mime_filename(part, filename.c_str());
            curl_mime_data(part, data.c_str(), data.size());
            curl_easy_setopt(this->curl, CURLOPT_MIMEPOST, mime);
        }

        curl_easy_setopt(this->curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(this->curl, CURLOPT_WRITEDATA, &this->buffer);
        curl_easy_setopt(this->curl, CURLOPT_USERAGENT, net_agent);
        curl_easy_setopt(this->curl, CURLOPT_WRITEFUNCTION, writer);
        curl_easy_setopt(this->curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(this->curl, CURLOPT_SSL_VERIFYHOST, 0L);
        curl_easy_setopt(this->curl, CURLOPT_TIMEOUT, 600L);

#if LIBCURL_VERSION_NUM >= 0x072000
        curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, xferinfo);
        curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &prog);
#else
        curl_easy_setopt(curl, CURLOPT_PROGRESSFUNCTION, older_progress);
        curl_easy_setopt(curl, CURLOPT_PROGRESSDATA, &prog);
#endif
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);

        this->result = curl_easy_perform(curl);
        fprintf(stderr, "\n");
        if (this->result != CURLE_OK)
            cout << "CURL ERROR: " << curl_easy_strerror(this->result) << endl;
#ifdef printOut
        cout << endl
             << url << endl
             << "	" << buffer << endl;
#endif
        if (mime)
            curl_mime_free(mime);
        curl_easy_reset(this->curl);
    }
}
