#pragma once
#include "common.h"
#include <curl/curl.h>

class Net
{
private:
    CURL* curl;
    CURLcode result;

public:
    Net();
    virtual ~Net();
    static void init();
    static void deinit();
    std::string urlEncode(std::string str);
    // виртуальные: тесты подменяют сеть моком (FakeNet), end-to-end без токенов
    virtual std::string send(std::string url, std::string params = "");
    virtual std::string send(std::string url, table_t param, bool post = true);
    virtual std::string upload(std::string url, std::string filename, std::string& data);
};
