#pragma once
// Мок сети для автотестов: записывает все запросы и отдаёт настраиваемые
// ответы по подстроке URL. Позволяет гонять полный конвейер бота
// (Worker → cmd → Event::send → Vk → Net) без сети и токенов VK.
#include "net.h"

#include <map>
#include <string>
#include <vector>

class FakeNet : public Net
{
public:
    struct Call
    {
        std::string url;
        table_t params; // для upload: {"filename": ...}, тело — в body
        std::string body;
    };

    std::vector<Call> calls;
    std::string defaultResponse = "{}"; // если подстрока не совпала
    std::map<std::string, std::string> responses; // подстрока URL -> ответ

    std::string send(std::string url, std::string params) override
    {
        calls.push_back({url, {}, params});
        return respond(url);
    }

    std::string send(std::string url, table_t param, bool post) override
    {
        (void)post;
        calls.push_back({url, param, ""});
        return respond(url);
    }

    std::string upload(std::string url, std::string filename, std::string& data) override
    {
        calls.push_back({url, { { "filename", filename } }, data});
        return respond(url);
    }

    // все вызовы, чей URL содержит подстроку
    std::vector<const Call*> find(const std::string& substr) const
    {
        std::vector<const Call*> out;
        for (const auto& c : calls)
            if (c.url.find(substr) != std::string::npos)
                out.push_back(&c);
        return out;
    }

    const Call* first(const std::string& substr) const
    {
        auto f = find(substr);
        return f.empty() ? nullptr : f.front();
    }

    // точное совпадение URL (методы VK идут как .../method/<метод> — подстрока
    // "messages.send" иначе матчит и "messages.sendMessageEventAnswer")
    const Call* firstExact(const std::string& url) const
    {
        for (const auto& c : calls)
            if (c.url == url)
                return &c;
        return nullptr;
    }

    void reset()
    {
        calls.clear();
    }

private:
    std::string respond(const std::string& url)
    {
        for (const auto& r : responses)
            if (url.find(r.first) != std::string::npos)
                return r.second;
        return defaultResponse;
    }
};
