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
    std::string urlEncode(const std::string& str);
    // виртуальные: тесты подменяют сеть моком (FakeNet), end-to-end без токенов
    virtual std::string send(const std::string& url, const std::string& params = "");
    virtual std::string send(const std::string& url, const table_t& param, bool post = true);
    virtual std::string upload(const std::string& url, const std::string& filename, const std::string& data);
};
