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
    CURL* curl;
    CURLcode result;

public:
    Net();
    virtual ~Net();
    static void init();
    std::string urlEncode(std::string str);
    std::string send(std::string url, std::string params = "");
    std::string send(std::string url, table_t param, bool post = true);
    std::string upload(std::string url, std::string filename, std::string& data);
};

#endif
