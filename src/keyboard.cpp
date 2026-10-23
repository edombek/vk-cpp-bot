#include "keyboard.h"

Keyboard::Keyboard(bool oneTime_, bool inlineKbd_)
    : oneTime(oneTime_)
    , inlineKbd(inlineKbd_)
    , buttons(json::array())
{
}

void Keyboard::ensureRow()
{
    if (buttons.empty())
        buttons.push_back(json::array());
}

void Keyboard::addAction(const json& action, const std::string& color)
{
    ensureRow();
    if (buttons.back().size() >= 5) // VK: ряд — максимум 5 кнопок («до 10 рядов по 5 кнопок»)
        buttons.push_back(json::array());
    json btn;
    btn["action"] = action;
    btn["color"] = color;
    buttons.back().push_back(btn);
}

Keyboard& Keyboard::button(const std::string& label, const std::string& payload, const std::string& color)
{
    json action;
    action["type"] = "callback";
    action["label"] = label;
    // VK требует payload формата JSON-строки в фигурных скобках (<= 255 символов)
    action["payload"] = json{{"cmd", payload}}.dump();
    addAction(action, color);
    return *this;
}

Keyboard& Keyboard::textButton(const std::string& label, const std::string& color)
{
    json action;
    action["type"] = "text";
    action["label"] = label;
    addAction(action, color);
    return *this;
}

Keyboard& Keyboard::link(const std::string& label, const std::string& url, const std::string& color)
{
    json action;
    action["type"] = "openlink";
    action["label"] = label;
    action["link"] = url;
    addAction(action, color);
    return *this;
}

Keyboard& Keyboard::row()
{
    buttons.push_back(json::array());
    return *this;
}

bool Keyboard::empty() const
{
    return buttons.empty();
}

void Keyboard::clear()
{
    buttons = json::array();
}

std::string Keyboard::dump() const
{
    json kbd;
    kbd["one_time"] = oneTime;
    kbd["inline"] = inlineKbd;
    kbd["buttons"] = json::array();
    for (const json& r : buttons)
        if (!r.empty()) // пустые ряды VK отвергает (error 911 «row contains too much columns»)
            kbd["buttons"].push_back(r);
    return kbd.dump();
}
