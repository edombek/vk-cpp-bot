#ifndef NET_H_INCLUDED
#define NET_H_INCLUDED
#include "common.h"
#ifdef __linux__
#include <curl/curl.h>
#elif _WIN32
#include "win32deps/include/curl/curl.h"
#endif

class Net {
private:
    std::string buffer;
    CURL* curl;
    CURLcode result;
    char errorBuffer[CURL_ERROR_SIZE];

public:
    Net();
    ~Net();
    static void init();
    std::string urlEncode(std::string str);
    std::string send(std::string url, std::string params);
    std::string send(std::string url, table_t param = {}, bool post = true);
};

#endif
