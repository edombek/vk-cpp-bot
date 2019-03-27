#ifndef NET_H_INCLUDED
#define NET_H_INCLUDED
#include "common.h"
#include <curl/curl.h>

class Net {
private:
    CURL* curl;
    CURLcode result;

public:
    Net();
    ~Net();
    static void init();
	static void deinit();
    std::string urlEncode(std::string str);
    std::string send(std::string url, std::string params = "");
    std::string send(std::string url, table_t param, bool post = true);
    std::string upload(std::string url, std::string filename, std::string& data);
};

#endif
