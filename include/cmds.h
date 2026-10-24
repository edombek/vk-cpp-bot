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
typedef void (*msg_func_t)(Event &eventIn, Event &eventOut, Worker *worker);

void help(Event &eventIn, Event &eventOut, Worker *worker);
void setCfg(Event &eventIn, Event &eventOut, Worker *worker);
void stat(Event &eventIn, Event &eventOut, Worker *worker);
void con(Event &eventIn, Event &eventOut, Worker *worker);
void upload(Event &eventIn, Event &eventOut, Worker *worker);
void setc(Event &eventIn, Event &eventOut, Worker *worker);
void rename(Event &eventIn, Event &eventOut, Worker *worker);
void videos(Event &eventIn, Event &eventOut, Worker *worker);
void weather(Event &eventIn, Event &eventOut, Worker *worker);
void gameF(Event &eventIn, Event &eventOut, Worker *worker);
void apod(Event &eventIn, Event &eventOut, Worker *worker);
void apoddate(Event &eventIn, Event &eventOut, Worker *worker);
void apodrand(Event &eventIn, Event &eventOut, Worker *worker);
void pycmd(Event &eventIn, Event &eventOut, Worker *worker);
void pywiki(Event &eventIn, Event &eventOut, Worker *worker);
void pix(Event &eventIn, Event &eventOut, Worker *worker);
void vox(Event &eventIn, Event &eventOut, Worker *worker);
void crt(Event &eventIn, Event &eventOut, Worker *worker);
void bc(Event &eventIn, Event &eventOut, Worker *worker);
void neon(Event &eventIn, Event &eventOut, Worker *worker);
