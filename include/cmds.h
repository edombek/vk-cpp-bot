#pragma once

// Реализации команд разнесены по модулям (src/cmds_*.cpp):
//   cmds_interface.cpp — /stat, /rename
//   cmds_utils.cpp     — /w
//   cmds_games.cpp     — /g, /bc
//   cmds_media.cpp     — /vid, /pix, /vox, /crt, /neon
//   cmds_apod.cpp      — /apod, /apoddate, /apodrand (категория «космос»)
//   cmds_admin.cpp     — /setc, /con, /u, /set, /py
// /help живёт в src/cmd.cpp — она строит справку из таблицы команд.

#include "events.h"
#include "worker.h"

// Единая сигнатура функций-команд (фаза 8, вместо макроса cmdHead):
//   eventIn  — входящее событие (парсинг, слова, юзер);
//   eventOut — формируемый ответ (наполнен из eventIn через getOut(); msg уже
//              содержит префикс имени), его send() дёргает диспетчер;
//   worker   — пул, вызвавший команду (nullptr в тестах).
// std::function (фаза 12): позволяет регистрировать в таблице и python-аддоны
// (лямбда со своим исходником), C++-функции биндятся неявно.
#include <functional>
typedef std::function<void(Event &eventIn, Event &eventOut, Worker *worker)> msg_func_t;

void help(Event &eventIn, Event &eventOut, Worker *worker);
void setCfg(Event &eventIn, Event &eventOut, Worker *worker);
void stat(Event &eventIn, Event &eventOut, Worker *worker);
void con(Event &eventIn, Event &eventOut, Worker *worker);
void upload(Event &eventIn, Event &eventOut, Worker *worker);
void setc(Event &eventIn, Event &eventOut, Worker *worker);
// renameUser (не rename): POSIX ::rename из <cstdio>/<filesystem> конфликтует
// с unqualified rename в таблице команд (имя команды при этом — «/rename»)
void renameUser(Event &eventIn, Event &eventOut, Worker *worker);
void videos(Event &eventIn, Event &eventOut, Worker *worker);
void weather(Event &eventIn, Event &eventOut, Worker *worker);
void gameF(Event &eventIn, Event &eventOut, Worker *worker);
void apod(Event &eventIn, Event &eventOut, Worker *worker);
void apoddate(Event &eventIn, Event &eventOut, Worker *worker);
void apodrand(Event &eventIn, Event &eventOut, Worker *worker);
void pycmd(Event &eventIn, Event &eventOut, Worker *worker);
void pywiki(Event &eventIn, Event &eventOut, Worker *worker);
void addons(Event &eventIn, Event &eventOut, Worker *worker);
void pyreload(Event &eventIn, Event &eventOut, Worker *worker);
void pix(Event &eventIn, Event &eventOut, Worker *worker);
void vox(Event &eventIn, Event &eventOut, Worker *worker);
void crt(Event &eventIn, Event &eventOut, Worker *worker);
void bc(Event &eventIn, Event &eventOut, Worker *worker);
void neon(Event &eventIn, Event &eventOut, Worker *worker);
