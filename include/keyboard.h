#pragma once
// Билдер клавиатур ВКонтакте (параметр keyboard метода messages.send).
// Формат: https://dev.vk.com/api/bots/getting-started (раздел «Клавиатура»):
// кнопки text отправляют label как сообщение, callback — payload событием
// message_event (текст скрыт); лимиты VK (dev.vk.ru «Клавиатура»): обычная —
// до 40 кнопок, 10 рядов по 5; inline — до 10 кнопок, 6 рядов по 5.
// Пустые ряды недопустимы — dump() их вырезает, addAction() переносит переполненный ряд.
#include "common.h"

#include <string>

class Keyboard
{
public:
    Keyboard(bool oneTime = false, bool inlineKbd = false);

    // callback-кнопка: payload — команда, которую получит бот (скрытый ввод)
    Keyboard& button(const std::string& label, const std::string& payload, const std::string& color = "secondary");
    // text-кнопка: пользователь получит label своим сообщением
    Keyboard& textButton(const std::string& label, const std::string& color = "secondary");
    // ссылка
    Keyboard& link(const std::string& label, const std::string& url, const std::string& color = "secondary");
    // начать новую строку кнопок
    Keyboard& row();

    bool empty() const;
    void clear();
    std::string dump() const; // JSON-строка для параметра keyboard

private:
    bool oneTime;
    bool inlineKbd;
    json buttons; // массив строк, каждая — массив кнопок
    void ensureRow();
    void addAction(const json& action, const std::string& color);
};
